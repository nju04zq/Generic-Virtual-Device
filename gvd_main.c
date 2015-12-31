/*
 * gvd_main.c
 *
 * By Qiang Zhou
 * October, 2014
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "gvd_tty.h"
#include "gvd_util.h"
#include "gvd_common.h"
#include "gvd_cli_tty.h"
#include "gvd_cli_tree.h"
#include "gvd_cli_parser.h"
#include "gvd_line_buffer.h"

#define INPUT_HISTORY_MAX_SIZE 16

#define CMD_HISTORY_MAX_SIZE 32

#define HOST_NAME_MAX_LEN 15

#define PS_MAX_LEN 63

typedef struct read_ctx_s {
    //typed keys for current command
    char cmd[CMD_MAX_LEN+1];
    //where the cursor locating
    int cmd_next_idx;
} read_ctx_t;

read_ctx_t read_ctx;

typedef int (*input_handler)(int);

typedef struct special_key_entry_s {
    int key;
    input_handler handler;
} special_key_entry_t;

static char host_name[HOST_NAME_MAX_LEN];

static char *banner = 
    "=======================================\n"
    "| GenericCallHome Virtual Device(GVD) |\n"
    "| By Qiang Zhou, October 2014         |\n"
    "=======================================\n";

static int input_process_enter(int input);
static int input_process_question_mark(int input);
static int input_process_tab(int input);
static int input_process_backspace(int input);
static int input_process_del(int input);
static int input_process_up(int input);
static int input_process_down(int input);
static int input_process_right(int input);
static int input_process_left(int input);
static int input_process_home(int input);
static int input_process_end(int input);
static int input_process_pgup(int input);
static int input_process_pgdown(int input);
static int input_process_resize(int input);
static int input_process_ctrl_c(int input);
static int input_process_ctrl_z(int input);

special_key_entry_t special_key_table[] = {
    {'?',               input_process_question_mark},
    {'\t',              input_process_tab}, //TAB
    {GVD_KEY_ENTER,     input_process_enter}, //Enter
    {GVD_KEY_BACKSPACE, input_process_backspace}, //backspace
    {GVD_KEY_DEL,       input_process_del}, //delete
    {GVD_KEY_UP,        input_process_up}, //up
    {GVD_KEY_DOWN,      input_process_down}, //down
    {GVD_KEY_LEFT,      input_process_left}, //left
    {GVD_KEY_RIGHT,     input_process_right}, //right
    {GVD_KEY_HOME,      input_process_home}, //home
    {GVD_KEY_END,       input_process_end}, //end
    {GVD_KEY_PGUP,      input_process_pgup}, //page up
    {GVD_KEY_PGDOWN,    input_process_pgdown}, //page down
    {GVD_RESIZE,        input_process_resize}, //resize window
    {GVD_CTRL_C,        input_process_ctrl_c}, //ctrl c
    {GVD_CTRL_Z,        input_process_ctrl_z}, //ctrl z
};

//0-9a-zA-Z not mentioned here. so many, but what's not included? ? 
static char *allowed_text_char_set = " ~`!@#$%^&*()-_=+[]{};:,.<>/|\\\"'";

static int input_history[INPUT_HISTORY_MAX_SIZE];

//the last entry is for garbage, see update cmd history
static char cmd_history[CMD_HISTORY_MAX_SIZE+1][CMD_MAX_LEN+1];
static int cmd_history_size = 0;
static int cmd_history_idx = 0;
static bool cur_cmd_in_history = FALSE;

extern gvd_tty_t gvd_tty;

static void
make_ps (char *ps)
{
    char *cli_mode_str;
    uint32_t len;

    gethostname(host_name, HOST_NAME_MAX_LEN);

    cli_mode_str = gvd_tty.cli_mode_p->mode_string;
    //two bytes reserved for "#"
    if (*cli_mode_str) {
        snprintf(ps, PS_MAX_LEN, "%s(%s)", host_name, cli_mode_str);
    } else {
        snprintf(ps, PS_MAX_LEN, "%s", host_name);
    }

    len = strlen(ps);
    ps[len++] = '#';
    ps[len++] = '\0';
    return;
}

uint32_t
get_ps_len (void)
{
    char ps[PS_MAX_LEN+1];
    uint32_t len = 0;

    make_ps(ps);
    len = strlen(ps);
    return len;
}

static void
print_ps (void)
{
    char ps[PS_MAX_LEN+1];

    make_ps(ps);
    printv(ps);
    return;
}

static void
update_input_history (int input)
{
    memcpy(&input_history[1], input_history,
           (INPUT_HISTORY_MAX_SIZE-1) * sizeof(input_history[0]));
    input_history[0] = input;
    return;
}

static void
move_cursor_cmd_home (void)
{
    int pos;

    pos = get_ps_len();
    tty_move_cursor_pos(pos);
    return;
}

static void
move_cursor_cmd_end (void)
{
    int pos;

    pos = get_ps_len() + strlen(read_ctx.cmd);
    tty_move_cursor_pos(pos);
    return;
}

//Remove the character before cursor in command line
static void
remove_parser_cmd_prev_char (void)
{
    int idx = read_ctx.cmd_next_idx;
    int cmd_len, cp_len;
    char *cmd;

    cmd = read_ctx.cmd;
    cmd_len = strlen(cmd);
    cp_len = cmd_len - idx;
    memmove(&cmd[idx-1], &cmd[idx], cp_len+1);
    read_ctx.cmd_next_idx--;

    return;
}

//remove the character in cursor
static void
remove_parser_cmd_cur_char (void)
{
    int idx = read_ctx.cmd_next_idx;
    int cmd_len, cp_len;
    char *cmd;

    cmd = read_ctx.cmd;
    cmd_len = strlen(cmd);
    cp_len = cmd_len - idx -1;
    memmove(&cmd[idx], &cmd[idx+1], cp_len+1);
    //reset the last char in cmd before del
    read_ctx.cmd[cmd_len-1] = '\0';

    return;
}

static void
insert_parser_cmd_char (char input)
{
    int idx = read_ctx.cmd_next_idx;
    int cmd_len, cp_len;
    char *cmd;

    cmd = read_ctx.cmd;
    cmd_len = strlen(cmd);
    cp_len = cmd_len - idx;
    memmove(&cmd[idx+1], &cmd[idx], cp_len+1);
    cmd[idx] = input;
    read_ctx.cmd_next_idx++;

    return;
}

static void
adjust_cursor_pos (void)
{
    int pos;

    pos = get_ps_len() + read_ctx.cmd_next_idx;
    tty_move_cursor_pos(pos);
    return;
}

static bool
is_cmd_empty (char *cmd)
{
    while (*cmd) {
        if (*cmd != ' ') {
            return FALSE;
        }
        cmd++;
    }
    return TRUE;
}

static void
update_cmd_history (void)
{
    int i;

    if (is_cmd_empty(read_ctx.cmd)) {
        return;
    }

    safe_strncpy(cmd_history[0], read_ctx.cmd, CMD_MAX_LEN);
    if (cmd_history_size < CMD_HISTORY_MAX_SIZE) {
        cmd_history_size++;
    }

    for (i = cmd_history_size; i > 0; i--) {
        safe_strncpy(cmd_history[i], cmd_history[i-1], CMD_MAX_LEN);
    }

    memset(cmd_history[0], 0, CMD_MAX_LEN);

    cmd_history_idx = 0;
    cur_cmd_in_history = FALSE;
    return;
}

static void
replace_parser_cmd (char *new_cmd)
{
    memset(read_ctx.cmd, 0, CMD_MAX_LEN+1);
    safe_strncpy(read_ctx.cmd, new_cmd, CMD_MAX_LEN);
    read_ctx.cmd_next_idx = strlen(read_ctx.cmd);
    return;
}

static void
add_cur_cmd_to_history (void)
{
    safe_strncpy(cmd_history[0], read_ctx.cmd, CMD_MAX_LEN);
    cur_cmd_in_history = TRUE;
    return;
}

static void
re_print_cmd (char *cmd)
{
    char ps[PS_MAX_LEN+1];

    make_ps(ps);
    replace_last_line(ps, read_ctx.cmd);
    return;
}

static int
input_process_enter (int input)
{
    int ret;
    char *output;

    update_cmd_history();

    move_cursor_cmd_end();
    printv("\n");

    ret = cli_parser_request(&gvd_tty, PARSER_REQ_EXEC, read_ctx.cmd, &output);
    if (output) {
        printv(output);
        free(output);
    }

    memset(read_ctx.cmd, 0, sizeof(read_ctx.cmd));
    read_ctx.cmd_next_idx = 0;

    print_ps();

    return ret;
}

static int
input_process_question_mark (int input)
{
    char *output = NULL;

    printv("?\n");

    (void)cli_parser_request(&gvd_tty, PARSER_REQ_QUERY, read_ctx.cmd, &output);
    if (output) {
        printv(output);
        free(output);
    }

    re_print_cmd(read_ctx.cmd);

    return PROCESS_CONTINUE;
}
static int
input_process_tab (int input)
{
    char *output = NULL, *cmd, *fill_output;

    (void)cli_parser_request(&gvd_tty, PARSER_REQ_AUTO_FILL,
                             read_ctx.cmd, &output);
    printv("\n");

    if (!output || !output[0]) {
        re_print_cmd(read_ctx.cmd);
        return PROCESS_CONTINUE;
    }

    cmd = output;
    fill_output = strchr(output, '\n');
    if (!fill_output) {
        free(output);
        re_print_cmd(read_ctx.cmd);
        return PROCESS_CONTINUE;
    }
    *(fill_output++) = '\0';

    replace_parser_cmd(cmd);
    if (fill_output[0] != '\0') {
        printv(fill_output);
    }
    re_print_cmd(read_ctx.cmd);

    free(output);
    return PROCESS_CONTINUE;
}

static bool
is_in_allowed_text_char_set (int input)
{
    char *str, ch;

    if ((input & ~(0x7f)) != 0) {
        return FALSE;
    }
    ch = input & 0x7f;

    if (isalnum(input)) {
        return TRUE;
    }

    str = allowed_text_char_set;
    while (*str) {
        if (ch == *str) {
            return TRUE;
        }
        str++;
    }

    return FALSE;
}

static int
input_process_text (int input)
{
    char input_ch;

    input_ch = input & 0x7f;

    if (strlen(read_ctx.cmd) >= CMD_MAX_LEN) {
        return PROCESS_CONTINUE;
    }

    insert_parser_cmd_char(input_ch);
    re_print_cmd(read_ctx.cmd);
    //after print, cursor comes to end of cmd, should adjust
    adjust_cursor_pos();
    
    return PROCESS_CONTINUE;
}

static int
input_process_backspace (int input)
{
    if (read_ctx.cmd_next_idx == 0) {
        return PROCESS_CONTINUE;
    }

    remove_parser_cmd_prev_char();
    re_print_cmd(read_ctx.cmd);
    //after print, cursor comes to end of cmd, should adjust
    adjust_cursor_pos();

    return PROCESS_CONTINUE;
}

static int
input_process_del (int input)
{
    int cmd_len;

    cmd_len = strlen(read_ctx.cmd);
    if (cmd_len == read_ctx.cmd_next_idx) {
        return PROCESS_CONTINUE;
    }

    remove_parser_cmd_cur_char();
    re_print_cmd(read_ctx.cmd);
    //after print, cursor comes to end of cmd, should adjust
    adjust_cursor_pos();

    return PROCESS_CONTINUE;
}

static int
input_process_home (int input)
{
    move_cursor_cmd_home();
    read_ctx.cmd_next_idx = 0;
    return PROCESS_CONTINUE;
}

static int
input_process_end (int input)
{
    move_cursor_cmd_end();
    read_ctx.cmd_next_idx = strlen(read_ctx.cmd);
    return PROCESS_CONTINUE;

}

static int
input_process_up (int input)
{
    int next_idx;

    next_idx = cmd_history_idx + 1;
    if (cmd_history_idx >= cmd_history_size) {
        return PROCESS_CONTINUE;
    }

    if (cmd_history_idx == 0 && !cur_cmd_in_history) {
        add_cur_cmd_to_history();
    }

    replace_parser_cmd(cmd_history[next_idx]);
    cmd_history_idx = next_idx;

    re_print_cmd(read_ctx.cmd);
    return PROCESS_CONTINUE;
}

static int
input_process_down (int input)
{
    int previous_idx;

    if (cmd_history_idx == 0) {
        return PROCESS_CONTINUE;
    }

    previous_idx = cmd_history_idx-1;

    replace_parser_cmd(cmd_history[previous_idx]);
    cmd_history_idx = previous_idx;

    re_print_cmd(read_ctx.cmd);
    return PROCESS_CONTINUE;
}

static int
input_process_right (int input)
{
    int cmd_len;

    cmd_len = strlen(read_ctx.cmd);
    if (read_ctx.cmd_next_idx == cmd_len) {
        return PROCESS_CONTINUE;
    }

    read_ctx.cmd_next_idx++; 
    tty_move_cursor_right();
    return PROCESS_CONTINUE;
}

static int
input_process_left (int input)
{
    if (read_ctx.cmd_next_idx == 0) {
        return PROCESS_CONTINUE;
    }

    read_ctx.cmd_next_idx--; 
    tty_move_cursor_left();
    return PROCESS_CONTINUE;
}

static int
input_process_pgup (int input)
{
    scroll_up_refresh();
    return PROCESS_CONTINUE;
}

static int
input_process_pgdown (int input)
{
    scroll_down_refresh();
    return PROCESS_CONTINUE;
}

static int
input_process_resize (int input)
{
    resize_refresh();
    return PROCESS_CONTINUE;
}

static int
input_process_ctrl_c (int input)
{
    printv("\n");
    replace_parser_cmd("");
    re_print_cmd(read_ctx.cmd);
    return PROCESS_CONTINUE;
}

static int
input_process_ctrl_z (int input)
{
    gvd_return_exec_cli_mode(&gvd_tty);
    printv("\n");
    replace_parser_cmd("");
    re_print_cmd(read_ctx.cmd);
    return PROCESS_CONTINUE;
}

static input_handler
find_special_key_handler (int input)
{
    int i, size;
    special_key_entry_t *entry_p;

    size = ARRAY_LEN(special_key_table);
    for (i = 0; i < size; i++) {
        entry_p = &special_key_table[i];
        if (entry_p->key == input) {
            return entry_p->handler;
        }
    }

    return NULL;
}

static int
input_key_process (int input)
{
    int ret = PROCESS_CONTINUE;
    input_handler handler = NULL;

    update_input_history(input);

    handler = find_special_key_handler(input);
    if (!handler && is_in_allowed_text_char_set(input)) {
        handler = input_process_text;
    }

    if (!handler) {
        // key not supported, ignore it
        return PROCESS_CONTINUE;
    }

    ret = handler(input);
    return ret;
}

static void
cli_read_init (void)
{
    memset(input_history, 0, sizeof(input_history));
    memset(&read_ctx, 0, sizeof(read_ctx_t));
    return;
}

static int
gvd_common_init (void)
{
    int rc;

    rc = gvd_init_cli_tree();
    if (rc != 0) {
        return -1;
    }

    return 0;
}

static void
printf_ps (void)
{
    char ps[PS_MAX_LEN+1];

    make_ps(ps);
    printf("%s", ps);
    return;
}

static void
autotest_read_in_cmd (char *cmd)
{
    uint32_t i = 0;
    int input;
    char ch;

    memset(cmd, 0, CMD_MAX_LEN+1);

    for (;;) {
        input = getchar();
        if (input == EOF) {
            break;
        }
        ch = (char)input;
        if (ch == '\n') {
            break;
        }
        if (i > CMD_MAX_LEN) {
            continue;
        }
        cmd[i++] = ch;
    }

    cmd[i] = '\0';
    return;
}

int is_autotest_mode = 0;

static void
autotest_mode (void)
{
    int rc, process_result;
    char cmd[CMD_MAX_LEN+1], *output;

    is_autotest_mode = 1;

    rc = gvd_common_init();
    if (rc == -1) {
        return;
    }

    printf("%s", banner);

    printf_ps();

    for (;;) {
        autotest_read_in_cmd(cmd);
        process_result = cli_parser_request(&gvd_tty, PARSER_REQ_EXEC,
                                            cmd, &output);
        if (output) {
            printf("%s", output);
            free(output);
        }
        if (process_result == PROCESS_EXIT) {
            break;
        }
        printf_ps();
    }

    return;
}

int
main (int argc, char *argv[])
{
    bool result;
    int input;
    int ret = 0, process_result;
    int rc;

    if (argv[1] && strcmp(argv[1], "autotest") == 0) {
        autotest_mode();
        return 0;
    }

    rc = gvd_common_init();
    if (rc == -1) {
        return -1;
    }

    rc = line_buffer_init();
    if (rc == -1) {
        return -1;
    }

    cli_read_init();

    printv(banner);

    print_ps();

    for (;;) {
        result = tty_read_one_key(&input);
        if (!result) {
            ret = -1;
            break;;
        }
        process_result = input_key_process(input);
        if (process_result == PROCESS_EXIT) {
            break;
        }
    }

    line_buffer_clean();
    return ret;
}

