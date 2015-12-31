#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "gvd_tty.h"
#include "gvd_util.h"
#include "gvd_common.h"
#include "gvd_cli_tree.h"
#include "gvd_cli_parser.h"

#define CLI_TOKEN_MAX_CNT 64

#define CLI_QUERY_INDENT_SPACE_CNT 2

#define GET_OBJ_STORE_IDX(param) ((param) & 0xff)

#define MAX_CHAR_PER_LINE 80

enum {
    PARSER_EXIT_CMD_OK = 0,
    PARSER_EXIT_NOT_FOUND,
    PARSER_EXIT_AMBIGUOUS,
    PARSER_EXIT_PARAM_FAIL,
};

typedef struct cli_split_info_s {
    char *token_starts[CLI_TOKEN_MAX_CNT];
    uint32_t token_cnt;
} cli_split_info_t;

typedef struct node_param_handler_s {
    int node_type;
    bool (*node_handler)(cli_tree_node_t *, cli_parser_info_t *, char *);
} node_param_handler_t;

static char *week_day[] = {"Monday", "Tuesday", "Wednesday", "Thursday",
                           "Friday", "Saturday", "Sunday"};

static bool
node_no_prefix_handler(cli_tree_node_t *node_p,
                       cli_parser_info_t *cpi_p, char *token);
static bool
node_default_prefix_handler(cli_tree_node_t *node_p,
                            cli_parser_info_t *cpi_p, char *token);
static bool
node_keyword_id_handler(cli_tree_node_t *node_p,
                        cli_parser_info_t *cpi_p, char *token);
static bool
node_num_param_handler(cli_tree_node_t *node_p,
                       cli_parser_info_t *cpi_p, char *token);
static bool
node_str_param_handler(cli_tree_node_t *node_p,
                       cli_parser_info_t *cpi_p, char *token);
static bool
node_week_day_param_handler(cli_tree_node_t *node_p,
                            cli_parser_info_t *cpi_p, char *token);
static bool
node_time_param_handler(cli_tree_node_t *node_p,
                        cli_parser_info_t *cpi_p, char *token);

static node_param_handler_t node_param_handlers[] = 
{
    {CLI_NODE_TYPE_NO,         node_no_prefix_handler},
    {CLI_NODE_TYPE_DEFAULT,    node_default_prefix_handler},
    {CLI_NODE_TYPE_KEYWORD_ID, node_keyword_id_handler},
    {CLI_NODE_TYPE_NUMBER,     node_num_param_handler},
    {CLI_NODE_TYPE_STRING,     node_str_param_handler},
    {CLI_NODE_TYPE_WEEK_DAY,   node_week_day_param_handler},
    {CLI_NODE_TYPE_TIME,       node_time_param_handler},
};

static char *
skip_spaces (char *cli)
{
    while (*cli == ' ') {
        cli++;
    }

    return cli;
}

static void
remove_trailing_spaces (char *cli)
{
    uint32_t len;
    int i;

    len = strlen(cli);
    for (i = len-1; i >= 0; i--) {
        if (cli[i] != ' ') {
            break;
        }
    }
    cli[i+1] = '\0';
    return;
}

static char *
preprocess_cli (char *cli)
{
    remove_trailing_spaces(cli);
    cli = skip_spaces(cli);
    return cli;
}

static int
split_cli (char *cli, cli_split_info_t *split_info_p, char **err_pos)
{
    char **starts_p = split_info_p->token_starts;
    uint32_t token_cnt = 1;
    bool escaped = FALSE, in_quotes = FALSE;

    *err_pos = NULL;
    memset(split_info_p, 0, sizeof(cli_split_info_t));

    *(starts_p++) = cli;

    while (*cli) {
        if (escaped) {
            escaped = FALSE;
            cli++;
            continue;
        }

        switch (*cli) {
        case ' ':
            if (in_quotes) {
                cli++;
            } else {
                *(cli++) = '\0';
                cli = skip_spaces(cli);
                if (token_cnt++ > CLI_TOKEN_MAX_CNT) return -1;
                *(starts_p++) = cli;
            }
            break;

        case '\\':
            escaped = TRUE;
            cli++;
            break;

        case '\"':
            if (in_quotes) {
                in_quotes = FALSE;
                cli++;
                if (*cli != ' ' && *cli != '\0') {
                    *err_pos = cli;
                    return -1;
                }
            } else {
                in_quotes = TRUE;
                cli++;
            }

            break;

        default:
            cli++;
            break;
        }
    }

    if (in_quotes) {
        *err_pos = cli;
        return -1;
    }

    split_info_p->token_cnt = token_cnt;
    return 0;
}

static void
mark_fail_token (cli_parser_info_t *cpi_p, char *token)
{
    print_buffer_t *output_p = &cpi_p->cli_output;
    uint32_t i, ps_len, token_pos, len;

    if (!token) {
        return;
    }

    ps_len = get_ps_len();
    token_pos = token - cpi_p->cli;

    len = ps_len + token_pos;
    for (i = 0; i < len; i++) {
        printb(output_p, " ");
    }

    printb(output_p, "^\n");
    return;
}

static void
handle_parser_err (int parser_exit_code,
                   cli_parser_info_t *cpi_p, char *token)
{
    print_buffer_t *output_p = &cpi_p->cli_output;

    mark_fail_token(cpi_p, token);

    switch (parser_exit_code) {
    case PARSER_EXIT_NOT_FOUND:
        printb(output_p, "Unrecognized command.\n\n");
        break;

    case PARSER_EXIT_AMBIGUOUS:
        printb(output_p, "Ambiguous command.\n\n");
        break;

    case PARSER_EXIT_PARAM_FAIL:
        printb(output_p, "Invalid parameter.\n\n");
        break;

    default:
        printb(output_p, "system error, unknown err code %d.\n\n",
               parser_exit_code);
        break;
    }

    return;
}

static bool
node_no_prefix_handler (cli_tree_node_t *node_p,
                        cli_parser_info_t *cpi_p, char *token)
{
    cpi_p->set_no = TRUE;
    return TRUE;
}

static bool
node_default_prefix_handler (cli_tree_node_t *node_p,
                             cli_parser_info_t *cpi_p, char *token)
{
    cpi_p->set_default = TRUE;
    return TRUE;
}

static bool
node_keyword_id_handler (cli_tree_node_t *node_p,
                         cli_parser_info_t *cpi_p, char *token)
{
    uint32_t idx;

    idx = GET_OBJ_STORE_IDX(node_p->param1);
    GET_OBJ(P_INT, idx) = node_p->flag;
    return TRUE;
}

static bool
node_num_param_handler (cli_tree_node_t *node_p,
                        cli_parser_info_t *cpi_p, char *token)
{
    uint32_t token_len, idx, i;
    int num;

    token_len = strlen(token);
    for (i = 0; i < token_len; i++) {
        if (!isdigit(token[i])) {
            return FALSE;
        }
    }

    num = atoi(token);
    if (num < node_p->min || num > node_p->max) {
        return FALSE;
    }

    idx = GET_OBJ_STORE_IDX(node_p->param1);
    GET_OBJ(P_INT, idx) = num;

    return TRUE;
}

static void
transform_escaped_char (char *src, char *dst)
{
    bool escaped = FALSE;

    for (; *src; src++) {
        if (*src == '\\') {
            escaped = TRUE;
            continue;
        }
        if (escaped) {
            escaped = FALSE;
            if (*src != '\\' && *src != '\"') {
                *(dst++) = '\\';
            }
        }
        *(dst++) = *src;
    }
    return;
}

static char *
preprocess_str_param (char *token)
{
    uint32_t token_len;
    char *str;

    if (*token == '\"') {
        token++;
    }

    token_len = strlen(token);
    if (token[token_len-1] == '\"') {
        token[token_len-1] = '\0';
    }

    token_len = strlen(token);
    str = calloc(token_len, sizeof(char));
    if (!str) {
        return NULL;
    }

    transform_escaped_char(token, str);

    return str;
}

static bool
node_str_param_handler (cli_tree_node_t *node_p,
                        cli_parser_info_t *cpi_p, char *token)
{
    int token_len;
    uint32_t idx;

    token = preprocess_str_param(token);
    if (!token) {
        return FALSE;
    }

    token_len = strlen(token);
    if (node_p->max > 0 && token_len > node_p->max) {
        free(token);
        return FALSE;
    }

    idx = GET_OBJ_STORE_IDX(node_p->param1);
    GET_OBJ(P_STRING, idx) = token;
    return TRUE;
}

static bool
node_week_day_param_handler (cli_tree_node_t *node_p,
                             cli_parser_info_t *cpi_p, char *token)
{
    uint32_t i, idx, len;

    len = strlen(token);
    for (i = 0; i < ARRAY_LEN(week_day); i++) {
        if (strncasecmp(token, week_day[i], len) == 0) {
            idx = GET_OBJ_STORE_IDX(node_p->param1);
            GET_OBJ(P_INT, idx) = i+1; //week day, 1-7
            return TRUE;
        }
    }
    return FALSE;
}

static bool
node_time_param_handler (cli_tree_node_t *node_p,
                         cli_parser_info_t *cpi_p, char *token)
{
    int hour, minute;
    uint32_t idx;

    hour = 0;
    while (*token) {
        if (*token == ':') {
            break;
        }
        if (isdigit(*token) == 0) {
            return FALSE;
        }
        hour = hour*10+(*token - '0');
        if (hour >= 24) {
            return FALSE;
        }
        token++;
    }

    if (*(token++) != ':') {
        return FALSE;
    }

    minute = 0;
    while (*token) {
        if (isdigit(*token) == 0) {
            return FALSE;
        }
        minute = minute*10+(*token - '0');
        if (minute >= 60) {
            return FALSE;
        }
        token++;
    }

    idx = GET_OBJ_STORE_IDX(node_p->param1);
    GET_OBJ(P_INT, idx) = hour;
    idx = GET_OBJ_STORE_IDX(node_p->param2);
    GET_OBJ(P_INT, idx) = minute;
    return TRUE;
}

static node_param_handler_t *
find_node_param_handler (int node_type)
{
    node_param_handler_t *param_handler_p;
    uint32_t i;

    for (i = 0; i < ARRAY_LEN(node_param_handlers); i++) {
        param_handler_p = &node_param_handlers[i];
        if (param_handler_p->node_type == node_type) {
            return param_handler_p;
        }
    }
    return NULL;
}

static bool
process_node_param (cli_tree_node_t *node_p,
                    cli_parser_info_t *cpi_p, char *token)
{
    bool result;
    node_param_handler_t *param_handler_p;

    param_handler_p = find_node_param_handler(node_p->node_type);
    if (!param_handler_p || !param_handler_p->node_handler) {
        return TRUE;
    }

    result = param_handler_p->node_handler(node_p, cpi_p, token);
    return result;
}

static cli_tree_node_t *
select_ifelse_node (cli_parser_info_t *cpi_p, cli_tree_node_t *node_p)
{
    int result;

    result = node_p->node_handler(cpi_p);
    if (result != 0) {
        return node_p->acc_p;
    } else {
        return node_p->alter_p;
    }
}

static bool
is_node_param_one (int node_type)
{
    if (node_type == CLI_NODE_TYPE_NUMBER ||
        node_type == CLI_NODE_TYPE_STRING ||
        node_type == CLI_NODE_TYPE_WEEK_DAY ||
        node_type == CLI_NODE_TYPE_TIME) {
        return TRUE;
    }

    return FALSE;
}

static bool
is_node_keyword_one (int node_type)
{
    if (node_type == CLI_NODE_TYPE_KEYWORD ||
        node_type == CLI_NODE_TYPE_KEYWORD_ID ||
        node_type == CLI_NODE_TYPE_NO ||
        node_type == CLI_NODE_TYPE_DEFAULT) {
        return TRUE;
    }

    return FALSE;
}

static cli_tree_node_t *
get_next_node (cli_parser_info_t *cpi_p, cli_tree_node_t *node_p)
{
    int node_type;

    node_type = node_p->node_type;

    if (node_type == CLI_NODE_TYPE_END ||
        node_type == CLI_NODE_TYPE_DEAD) {
        return NULL;
    }

    if (node_type == CLI_NODE_TYPE_IFELSE) {
        return select_ifelse_node(cpi_p, node_p);
    }

    if (node_type == CLI_NODE_TYPE_HELP) {
        return node_p->acc_p;
    }

    return node_p = node_p->alter_p;
}

static cli_tree_node_t *
get_string_node (cli_parser_info_t *cpi_p, cli_tree_node_t *node_p,
                 uint32_t *match_cnt_p)
{
    *match_cnt_p = 0;
    while (node_p) {
        if (node_p->node_type == CLI_NODE_TYPE_STRING) {
            *match_cnt_p = 1;
            return node_p;
        }
        node_p = get_next_node(cpi_p, node_p);
    }
    return NULL;
}

static cli_tree_node_t *
get_match_node (cli_parser_info_t *cpi_p, cli_tree_node_t *node_p,
                char *token, uint32_t *match_cnt_p)
                
{
    cli_tree_node_t *node_param_p = NULL, *node_match_p = NULL;
    uint32_t token_len;
    int node_type;

    if (*token == '\"') {
        node_match_p = get_string_node(cpi_p, node_p, match_cnt_p);
        return node_match_p;
    }

    *match_cnt_p = 0;
    token_len = strlen(token);
    while (node_p) {
        node_type = node_p->node_type;

        if (is_node_param_one(node_type)) {
            node_param_p = node_p;
        }

        if (is_node_keyword_one(node_type)) {
            if (!strncasecmp(token, node_p->keyword, token_len)) { 
                (*match_cnt_p)++;
                node_match_p = node_p;
            }
        }
        
        node_p = get_next_node(cpi_p, node_p);
    }

    if (!node_match_p) {
        node_match_p = node_param_p;
    }

    return node_match_p;
}

static cli_tree_node_t *
parse_cli (cli_parser_info_t *cpi_p,
           cli_split_info_t *split_info_p)
{
    char *token;
    bool result;
    uint32_t i, match_cnt;
    cli_tree_node_t *cur_node_p;
    int parser_exit_code = PARSER_EXIT_CMD_OK;

    cur_node_p = cpi_p->root_node_p;
    for (i = 0; i < split_info_p->token_cnt; i++) {
        token = split_info_p->token_starts[i];

        cur_node_p = get_match_node(cpi_p, cur_node_p, token, &match_cnt);
        if (!cur_node_p) {
            parser_exit_code = PARSER_EXIT_NOT_FOUND;
            break;
        }
        if (match_cnt > 1) {
            parser_exit_code = PARSER_EXIT_AMBIGUOUS;
            break;
        }

        result = process_node_param(cur_node_p, cpi_p, token);
        if (!result) {
            parser_exit_code = PARSER_EXIT_PARAM_FAIL;
            break;
        }

        cur_node_p = cur_node_p->acc_p;
    }

    if (parser_exit_code == PARSER_EXIT_CMD_OK) {
        return cur_node_p;
    } else {
        handle_parser_err(parser_exit_code, cpi_p, token);
        return NULL;
    }
}

static int
exec_cli (cli_parser_info_t *cpi_p, cli_tree_node_t *node_p)
{
    print_buffer_t *output_p = &cpi_p->cli_output;

    while (node_p) {
        if (node_p->node_type == CLI_NODE_TYPE_END) {
            break;
        }
        node_p = get_next_node(cpi_p, node_p);
    }

    if (!node_p) {
        printb(output_p, "Incomplete command.\n\n");
        return PROCESS_CONTINUE;
    }

    cpi_p->flag = node_p->flag;
    node_p->cli_handler(cpi_p);
    if (cpi_p->process_result != PROCESS_CONTINUE) {
        return cpi_p->process_result;
    }

    if (node_p->submode_type != CLI_MODE_NONE &&
        (!cpi_p->set_no && !cpi_p->set_default)) {
        gvd_enter_lower_cli_mode(cpi_p->tty_p, node_p->submode_type);
    }

    return PROCESS_CONTINUE;
}

static int
cli_parser_exec (cli_parser_info_t *cpi_p)
{
    char *cli = cpi_p->cli, *err_pos;
    cli_split_info_t split_info;
    print_buffer_t *output_p = &cpi_p->cli_output;
    cli_tree_node_t *node_p;
    int ret;

    cli = preprocess_cli(cli);
    if (*cli == '\0') {
        return PROCESS_CONTINUE;
    }

    ret = split_cli(cli, &split_info, &err_pos);
    if (ret == -1) {
        mark_fail_token(cpi_p, err_pos);
        printb(output_p, "Invalid command.\n\n");
        return PROCESS_CONTINUE;
    }

    node_p = parse_cli(cpi_p, &split_info);
    if (!node_p) {
        return PROCESS_CONTINUE;
    }

    ret = exec_cli(cpi_p, node_p);

    return ret;
}

static void
insert_help_node (cli_tree_node_t **head_node_pp,
                  cli_tree_node_t *cur_node_p,
                  cli_tree_node_t *prev_node_p,
                  cli_tree_node_t *next_node_p)
{
    cur_node_p->help_prev_p = prev_node_p;
    cur_node_p->help_next_p = next_node_p;
    if (!prev_node_p) {
        *head_node_pp = cur_node_p;
    } else {
        prev_node_p->help_next_p = cur_node_p;
    }
    if (next_node_p) {
        next_node_p->help_prev_p = cur_node_p;
    }
    return;
}

static void
insert_help_node_list_tail (cli_tree_node_t **head_node_pp,
                            cli_tree_node_t *node_p)
{
    cli_tree_node_t *cur_node_p, *prev_node_p = NULL;

    cur_node_p = *head_node_pp;
    while (cur_node_p) {
        prev_node_p = cur_node_p;
        cur_node_p = cur_node_p->help_next_p;
    }

    insert_help_node(head_node_pp, node_p, prev_node_p, cur_node_p);
}

static void
insert_help_node_list (cli_tree_node_t **head_node_pp,
                       cli_tree_node_t *node_p)
{
    cli_tree_node_t *cur_node_p, *prev_node_p = NULL;

    if (node_p->node_type == CLI_NODE_TYPE_END) {
        insert_help_node_list_tail(head_node_pp, node_p);
        return;
    }

    // sort from small to big, ensure help node is the first one
    cur_node_p = *head_node_pp;
    while (cur_node_p) {
        if (cur_node_p->node_type == CLI_NODE_TYPE_END) {
            break;
        }
        if (strcmp(node_p->keyword, cur_node_p->keyword) < 0) {
            break;
        }
        prev_node_p = cur_node_p;
        cur_node_p = cur_node_p->help_next_p;
    }

    insert_help_node(head_node_pp, node_p, prev_node_p, cur_node_p);
    return;
}

static void
clean_help_node_list (cli_tree_node_t **head_node_pp)
{
    cli_tree_node_t *cur_node_p, *next_node_p;

    cur_node_p = *head_node_pp;
    while (cur_node_p) {
        next_node_p = cur_node_p->help_next_p;
        cur_node_p->help_prev_p = NULL;
        cur_node_p->help_next_p = NULL;
        cur_node_p = next_node_p;
    }

    *head_node_pp = NULL;
    return;
}

static bool
is_node_help (cli_tree_node_t *node_p, char *token)
{
    uint32_t len;
    int node_type = node_p->node_type;

    if (node_type == CLI_NODE_TYPE_DEAD || node_type == CLI_NODE_TYPE_IFELSE) {
        return FALSE;
    }

    if (*token == '\0') {
        return TRUE;
    }

    if (is_node_keyword_one(node_type) == FALSE) {
        return FALSE;
    }

    len = strlen(token);
    if (strncasecmp(node_p->keyword, token, len) == 0) {
        return TRUE;
    }
    return FALSE;
}

static cli_tree_node_t *
collect_help_node (cli_parser_info_t *cpi_p, cli_tree_node_t *node_p,
                   char *last_token)
{
    cli_tree_node_t *head_node_p = NULL;

    while (node_p) {
        if (is_node_help(node_p, last_token)) {
            insert_help_node_list(&head_node_p, node_p);
        }
        node_p = get_next_node(cpi_p, node_p);
    }

    return head_node_p;
}

static void
print_spaces (print_buffer_t *output_p, uint32_t space_cnt)
{
    uint32_t i;

    for (i = 0; i < space_cnt; i++) {
        printb(output_p, " ");
    }
    return;
}

static uint32_t
get_help_node_list_keyword_max_len (cli_tree_node_t *head_node_p)
{
    cli_tree_node_t *node_p = head_node_p;
    uint32_t max_len, len;

    max_len = strlen(node_p->keyword);
    node_p = node_p->help_next_p;
    while (node_p) {
        len = strlen(node_p->keyword);
        if (len > max_len) {
            max_len = len;
        }
        node_p = node_p->help_next_p;
    }

    return max_len;
}

static char *
cut_help_string (char *str, uint32_t line_left, uint32_t *print_len_p)
{
    char *p, *line_end, *next_line;

    if (strlen(str) <= line_left) {
        *print_len_p = strlen(str);
        return NULL;
    }

    p = str + line_left;

    for (; p > str; p--) {
        if (*p == ' ') {
            break;
        }
    }
    if (p == str) {
        *print_len_p = line_left;
        return (str+line_left);
    }

    next_line = p;

    for (; p > str; p--) {
        if (*p != ' ') {
            break;
        }
    }
    line_end = p;
    *print_len_p = (line_end-str+1);

    while (*next_line) {
        if (*next_line != ' ') {
            break;
        }
        next_line++;
    }

    return next_line;
}

static void
cli_query_print_help_string (print_buffer_t *output_p, char *help_str,
                             uint32_t indent)
{
    char line[MAX_CHAR_PER_LINE+1], *next_line;
    uint32_t line_left, str_left, print_len;
    bool first_line = TRUE;

    if (indent >= MAX_CHAR_PER_LINE) {
        return;
    }

    line_left = MAX_CHAR_PER_LINE - indent;
    str_left = strlen(help_str);

    if (str_left == 0) {
        printb(output_p, "\n");
        return;
    }

    while (str_left > 0) {
        if (!first_line) {
            print_spaces(output_p, indent);
        }
        next_line = cut_help_string(help_str, line_left, &print_len);
        safe_strncpy(line, help_str, print_len);
        printb(output_p, "%s\n", line);
        if (!next_line) {
            break;
        }
        help_str = next_line;
        str_left = strlen(help_str);
        first_line = FALSE;
    }

    return;
}

static void
cli_query_print_help_node_list (cli_parser_info_t *cpi_p,
                                cli_tree_node_t *head_node_p)
{
    print_buffer_t *output_p = &cpi_p->cli_output;
    cli_tree_node_t *node_p;
    char *mode_help_string;
    uint32_t max_keyword_len, space_cnt, indent;

    mode_help_string = cpi_p->tty_p->cli_mode_p->help_string;
    printb(output_p, "%s commands:\n", mode_help_string);

    // print help node first
    node_p = head_node_p;
    if (node_p->node_type == CLI_NODE_TYPE_HELP) {
        printb(output_p, node_p->help_string);
        if (node_p->node_handler) {
            (void)node_p->node_handler(cpi_p);
        }
        node_p = node_p->help_next_p;
    }

    max_keyword_len = get_help_node_list_keyword_max_len(head_node_p);
    indent = max_keyword_len + CLI_QUERY_INDENT_SPACE_CNT*2;
    while (node_p) {
        print_spaces(output_p, CLI_QUERY_INDENT_SPACE_CNT);
        printb(output_p, node_p->keyword);
        space_cnt = max_keyword_len - strlen(node_p->keyword);
        print_spaces(output_p, space_cnt);
        print_spaces(output_p, CLI_QUERY_INDENT_SPACE_CNT);
        cli_query_print_help_string(output_p, node_p->help_string, indent);
        node_p = node_p->help_next_p;
    }

    printb(output_p, "\n");
    return;
}

static void
cli_query_print_help (cli_parser_info_t *cpi_p, cli_tree_node_t *node_p,
                      char *last_token)
{
    cli_tree_node_t *head_node_p;

    head_node_p = collect_help_node(cpi_p, node_p, last_token);
    if (!head_node_p) {
        return;
    }

    cli_query_print_help_node_list(cpi_p, head_node_p);

    clean_help_node_list(&head_node_p);
    return;
}

static cli_tree_node_t *
cli_query_get_help_node (cli_parser_info_t *cpi_p,
                         cli_split_info_t *split_info_p)
{
    cli_tree_node_t *node_p;

    if (split_info_p->token_cnt == 0) {
        return cpi_p->root_node_p;
    }

    node_p = parse_cli(cpi_p, split_info_p);
    return node_p;
}

static void
cli_parser_query (cli_parser_info_t *cpi_p)
{
    char *cli = cpi_p->cli, *err_pos, *last_token = NULL;
    uint32_t len;
    cli_split_info_t split_info;
    print_buffer_t *output_p = &cpi_p->cli_output;
    cli_tree_node_t *node_p;
    int ret;

    len = strlen(cli);
    if (len > 0 && cli[len-1] == ' ') {
        last_token = "";
    }

    cli = preprocess_cli(cli);
    if (*cli == '\0') {
        cli_query_print_help(cpi_p, cpi_p->root_node_p, "");
        return;
    }

    ret = split_cli(cli, &split_info, &err_pos);
    if (ret == -1) {
        mark_fail_token(cpi_p, err_pos);
        printb(output_p, "Invalid command.\n\n");
        return;
    }
    
    if (!last_token) {
        last_token = split_info.token_starts[split_info.token_cnt-1];
        split_info.token_cnt--;
        if (*last_token == '\"') {
            return;
        }
    }

    node_p = cli_query_get_help_node(cpi_p, &split_info);
    if (!node_p) {
        return;
    }

    cli_query_print_help(cpi_p, node_p, last_token);

    return;
}

static void
cli_autofill_print_help_node_list (cli_parser_info_t *cpi_p,
                                   cli_tree_node_t *head_node_p)
{
    print_buffer_t *output_p = &cpi_p->cli_output;
    cli_tree_node_t *node_p;

    node_p = head_node_p;

    printb(output_p, "\n");

    while (node_p) {
        if (node_p != head_node_p) {
            print_spaces(output_p, CLI_QUERY_INDENT_SPACE_CNT);
        }
        printb(output_p, node_p->keyword);
        node_p = node_p->help_next_p;
    }

    printb(output_p, "\n");
    return;
}

static void
autofill_cli (cli_parser_info_t *cpi_p, char *last_fill_token)
{
    char *cli_org = cpi_p->cli_org, *cli_end, *str;
    print_buffer_t *output_p = &cpi_p->cli_output;
    uint32_t len;

    len = strlen(cli_org);
    cli_end = cli_org+len;

    while (cli_end >= cli_org) {
        if (*cli_end == ' ') {
            break;
        }
        cli_end--;
    }

    cli_end++;
    len = cli_end-cli_org;
    if (len == 0) {
        str = safe_clone("", 0);
    } else {
        str = safe_clone(cli_org, len);
    }
    if (!str) {
        return;
    }

    printb(output_p, str);
    printb(output_p, "%s", last_fill_token);

    free(str);
    return;
}

static char *
make_last_fill_token (cli_tree_node_t *head_node_p)
{
    cli_tree_node_t *node_p;
    bool all_match;
    uint32_t len = 0;
    char *first_keyword = head_node_p->keyword, *str;
    char *last_fill_token;

    str = first_keyword;
    while (*str) {
        all_match = TRUE;
        node_p = head_node_p->help_next_p;
        while (node_p) {
            if (node_p->keyword[len] != *str) {
                all_match = FALSE;
                break;
            }
            node_p = node_p->help_next_p;
        }
        if (!all_match) {
            break;
        }
        len++;
        str++;
    }

    last_fill_token = safe_clone(first_keyword, len);
    return last_fill_token;
}

static void
cli_autofill_print_help (cli_parser_info_t *cpi_p, cli_tree_node_t *node_p,
                         char *last_token)
{
    print_buffer_t *output_p = &cpi_p->cli_output;
    cli_tree_node_t *head_node_p;
    char *last_fill_token;

    head_node_p = collect_help_node(cpi_p, node_p, last_token);
    if (!head_node_p) {
        return;
    }

    if (head_node_p->help_next_p == NULL) {
        autofill_cli(cpi_p, head_node_p->keyword);
        printb(output_p, " \n");
        return;
    }

    last_fill_token = make_last_fill_token(head_node_p);
    if (!last_fill_token) {
        return;
    }

    autofill_cli(cpi_p, last_fill_token);
    free(last_fill_token);

    cli_autofill_print_help_node_list(cpi_p, head_node_p);

    clean_help_node_list(&head_node_p);
    return;
}

static void
cli_parser_auto_fill (cli_parser_info_t *cpi_p)
{
    char *cli = cpi_p->cli, *err_pos, *last_token;
    uint32_t len;
    cli_split_info_t split_info;
    print_buffer_t *output_p = &cpi_p->cli_output;
    cli_tree_node_t *node_p;
    int ret;

    len = strlen(cli);
    if (len > 0 && cli[len-1] == ' ') {
        return;
    }

    cli = preprocess_cli(cli);
    if (*cli == '\0') {
        return;
    }

    ret = split_cli(cli, &split_info, &err_pos);
    if (ret == -1) {
        mark_fail_token(cpi_p, err_pos);
        printb(output_p, "Invalid command.\n\n");
        return;
    }

    last_token = split_info.token_starts[split_info.token_cnt-1];
    split_info.token_cnt--;
    if (*last_token == '\"') {
        return;
    }

    node_p = cli_query_get_help_node(cpi_p, &split_info);
    if (!node_p) {
        return;
    }

    cli_autofill_print_help(cpi_p, node_p, last_token);

    return;
}

static bool
cli_start_with_do (char *cli)
{
    cli = skip_spaces(cli);

    if (*(cli++) != 'd') {
        return FALSE;
    }
    if (*(cli++) != 'o') {
        return FALSE;
    }
    if (*cli != ' ' && *cli != '\0') {
        return FALSE;
    }

    return TRUE;
}

static void
skip_first_token (char *cli_start)
{
    char *cli = cli_start;

    cli = skip_spaces(cli);

    while (*cli) {
        if (*cli == ' ') {
            break;
        }
        cli++;
    }

    memset(cli_start, ' ', cli-cli_start);
    return;
}

static int
init_cli_parser_info (cli_parser_info_t *cpi_p, char *cli_in)
{
    int rc;
    char *cli;

    cli = safe_clone(cli_in, 0);
    if (!cli) {
        return -1;
    }

    rc = alloc_print_buffer(&cpi_p->cli_output);
    if (rc == -1) {
        free(cli);
        return -1;
    }

    cpi_p->flag = 0;
    cpi_p->set_no = 0;
    cpi_p->set_default = 0;
    cpi_p->root_node_p = cpi_p->tty_p->cli_mode_p->root_node_p;
    cpi_p->cli = cli;
    cpi_p->cli_org = cli_in;
    cpi_p->process_result = PROCESS_CONTINUE;
    memset(cpi_p->P_INT_buf, 0, P_MAX*sizeof(int));
    memset(cpi_p->P_STRING_buf, 0, P_MAX*sizeof(char *));

    if (cli_start_with_do(cli_in)) {
        cpi_p->root_node_p = gvd_get_exec_cli_mode_root();
        skip_first_token(cpi_p->cli);
    }
    return 0;
}

static void
clean_cli_parser_info (cli_parser_info_t *cpi_p)
{
    uint32_t i;

    for (i = 0; i < P_MAX; i++) {
        if (cpi_p->P_STRING_buf[i]) {
            free(cpi_p->P_STRING_buf[i]);
        }
    }

    free(cpi_p->cli);

    if (cpi_p->cli_output.buf) {
        free(cpi_p->cli_output.buf);
    }

    cpi_p->flag = 0;
    cpi_p->set_no = 0;
    cpi_p->set_default = 0;
    cpi_p->root_node_p = NULL;
    cpi_p->cli = NULL;
    memset(cpi_p->P_INT_buf, 0, P_MAX*sizeof(int));
    memset(cpi_p->P_STRING_buf, 0, P_MAX*sizeof(char *));
    memset(&cpi_p->cli_output, 0, sizeof(print_buffer_t));

    return;
}

int
cli_parser_request (gvd_tty_t *tty_p, int req_code, char *cli_in, char **output)
{
    cli_parser_info_t *cpi_p = &tty_p->cpi;
    int ret;

    *output = NULL;

    ret = init_cli_parser_info(cpi_p, cli_in);
    if (ret == -1) {
        return PROCESS_CONTINUE;
    }

    if (cpi_p->root_node_p == NULL) {
        return PROCESS_CONTINUE;
    }

    switch (req_code) {
    case PARSER_REQ_EXEC:
        ret = cli_parser_exec(cpi_p);
        break;

    case PARSER_REQ_QUERY:
        cli_parser_query(cpi_p);
        ret = PROCESS_CONTINUE;
        break;

    case PARSER_REQ_AUTO_FILL:
        cli_parser_auto_fill(cpi_p);
        ret = PROCESS_CONTINUE;
        break;

    default:
        break;
    }

    *output = safe_clone(cpi_p->cli_output.buf, 0);

    clean_cli_parser_info(cpi_p);
    return ret;
}

char *
gvd_run_cli (gvd_tty_t *tty_p, char *cli)
{
    char *output;

    (void)cli_parser_request(tty_p, PARSER_REQ_EXEC, cli, &output);
    return output;
}

