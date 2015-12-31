#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "gvd_tty.h"
#include "gvd_util.h"
#include "gvd_common.h"
#include "gvd_cfg_sys.h"
#include "gvd_cli_tree.h"
#include "gvd_cli_example_tree.h"

#define EXIT_HELP_STR_MAX_LEN 63
#define NUM_NODE_HELP_STR_MAX_LEN 15

cli_tree_node_t node_dead = {NULL, NULL, NULL, NULL, NULL, NULL,
                             0, 0, -1, -1, NULL, CLI_NODE_TYPE_DEAD,
                             CLI_MODE_NONE, 0};

static cli_mode_def_t cli_mode_defs[] =
{
    {
        CLI_MODE_EXEC, CLI_MODE_NONE, "",
        "Exec"
    },
    {
        CLI_MODE_SHELL, CLI_MODE_EXEC, "shell",
        "Shell"
    },
    {
        CLI_MODE_CONFIG, CLI_MODE_EXEC, "config",
        "Configure"
    },
    {
        CLI_MODE_CONFIG_GVD, CLI_MODE_CONFIG, "gvd",
        "Configure GVD"
    },
};

static cli_tree_root_link_t *tree_root_links[] =
{
    &link_name(node_quit, CLI_MODE_EXEC),
    &link_name(node_shell_exec, CLI_MODE_SHELL),
};

static cli_mode_t *exec_mode_p = NULL;
static cli_mode_t *cli_mode_buf_p = NULL;
static uint32_t cli_mode_cnt;

gvd_tty_t gvd_tty;

cli_mode_t *
gvd_find_cli_mode (int mode)
{
    uint32_t i;
    cli_mode_t *cli_mode_p;

    cli_mode_p = cli_mode_buf_p;
    for (i = 0; i < cli_mode_cnt; i++) {
        if (cli_mode_buf_p[i].mode == mode) {
            return &cli_mode_buf_p[i];
        }
    }
    return NULL;
}

static bool
is_cli_mode_in_config (int mode)
{
    if (mode == CLI_MODE_EXEC || mode == CLI_MODE_SHELL) {
        return FALSE;
    }
    return TRUE;
}

cli_tree_node_t *
gvd_get_exec_cli_mode_root (void)
{
    return exec_mode_p->root_node_p;
}

cli_mode_t *
gvd_get_exec_cli_mode (void)
{
    return exec_mode_p;
}

static int
link_cli_mode (cli_mode_t *cli_mode_p, uint32_t cnt)
{
    uint32_t i;

    for (i = 0; i < cnt; i++, cli_mode_p++) {
        if (cli_mode_p->parent_mode == CLI_MODE_NONE) {
            continue;
        }
        cli_mode_p->parent_p = gvd_find_cli_mode(cli_mode_p->parent_mode);
        if (!cli_mode_p->parent_p) {
            return -1;
        }
    }
    return 0;
}

static int
init_cli_mode (void)
{
    uint32_t i, mode_cnt;
    int rc;

    mode_cnt = ARRAY_LEN(cli_mode_defs);
    cli_mode_buf_p = calloc(mode_cnt, sizeof(cli_mode_t));
    if (!cli_mode_buf_p) {
        return -1;
    }
    cli_mode_cnt = mode_cnt;

    for (i = 0; i < mode_cnt; i++) {
        cli_mode_buf_p[i].mode = cli_mode_defs[i].mode;
        cli_mode_buf_p[i].parent_mode = cli_mode_defs[i].parent_mode;
        cli_mode_buf_p[i].mode_string = cli_mode_defs[i].mode_string;
        cli_mode_buf_p[i].help_string = cli_mode_defs[i].help_string;
    }

    rc = link_cli_mode(cli_mode_buf_p, mode_cnt);
    if (rc == -1) {
        free(cli_mode_buf_p);
        return -1;
    }

    exec_mode_p = gvd_find_cli_mode(CLI_MODE_EXEC);
    if (!exec_mode_p) {
        free(cli_mode_buf_p);
        return -1;
    }

    return 0;
}

static void
add_node_to_org (cli_tree_node_t *org_root_p, cli_tree_node_t *add_root_p)
{
    cli_tree_node_t *org_alter_p;

    org_alter_p = org_root_p->alter_p;
    org_root_p->alter_p = add_root_p;
    add_root_p->alter_p = org_alter_p;
    return;
}

static bool
is_node_keyword (int node_type)
{
    if (node_type == CLI_NODE_TYPE_KEYWORD ||
        node_type == CLI_NODE_TYPE_KEYWORD_ID) {
        return TRUE;
    }
    return FALSE;
}

static bool
is_node_same (cli_tree_node_t *node1_p, cli_tree_node_t *node2_p)
{
    if (node1_p->node_type != node2_p->node_type) {
        return FALSE;
    }

    if (is_node_keyword(node1_p->node_type) == FALSE) {
        return TRUE;
    }

    if (strcmp(node1_p->keyword, node2_p->keyword) == 0) {
        return TRUE;
    }

    return FALSE;
}

static cli_tree_node_t *
find_node_in_org_root (cli_tree_node_t *org_root_p,
                       cli_tree_node_t *add_root_p)
{
    cli_tree_node_t *node_p;

    node_p = org_root_p;
    while (node_p) {
        if (is_node_same(node_p, add_root_p)) {
            return node_p;
        }

        node_p = node_p->alter_p;
    }

    return NULL;
}

static void
merge_cli_tree (cli_tree_node_t *org_root_p, cli_tree_node_t *add_root_p)
{
    cli_tree_node_t *node_p, *next_node_p, *org_exist_node_p;

    node_p = add_root_p;
    while (node_p) {
        if (node_p->node_type == CLI_NODE_TYPE_END) {
            return;
        }

        next_node_p = node_p->alter_p;

        org_exist_node_p = find_node_in_org_root(org_root_p, node_p);
        if (!org_exist_node_p) {
            add_node_to_org(org_root_p, node_p);
        } else {
            merge_cli_tree(org_exist_node_p->acc_p, node_p->acc_p);
        }

        node_p = next_node_p;
    }

    return;
}

static int
link_one_root_node (cli_tree_node_t *root_p, int mode)
{
    cli_mode_t *cli_mode_p;

    cli_mode_p = gvd_find_cli_mode(mode);
    if (!cli_mode_p) {
        return -1;
    }

    if (cli_mode_p->root_node_p) {
        merge_cli_tree(cli_mode_p->root_node_p, root_p);
    } else {
        cli_mode_p->root_node_p = root_p;
    }
    return 0;
}

int
cli_link_root_nodes (cli_tree_root_link_t **link_p, uint32_t cnt)
{
    uint32_t i;
    int rc;

    for (i = 0; i < cnt; i++) {
        rc = link_one_root_node(link_p[i]->root_p, link_p[i]->cli_mode);
        if (rc == -1) {
            return -1;
        }
    }

    return 0;
}

static int
cli_link_default_root_nodes (void)
{
    int rc;

    rc = cli_link_root_nodes(tree_root_links, ARRAY_LEN(tree_root_links));
    if (rc == -1) {
        return -1;
    }

    return 0;
}

static cli_tree_node_t *
clone_cli_node (cli_tree_node_t *node_p)
{
    cli_tree_node_t *node_clone_p;

    node_clone_p = calloc(1, sizeof(cli_tree_node_t));
    if (!node_clone_p) {
        return NULL;
    }

    memcpy(node_clone_p, node_p, sizeof(cli_tree_node_t));
    return node_clone_p;
}

static void
make_exit_help_str (cli_mode_t *cli_mode_p, cli_tree_node_t *node_p)
{
    char *str;

    str = calloc(EXIT_HELP_STR_MAX_LEN+1, sizeof(char));
    if (!str) {
        return;
    }

    snprintf(str, EXIT_HELP_STR_MAX_LEN+1, node_p->help_string,
             cli_mode_p->help_string);

    node_p->help_string = str;
    return;
}

static void
add_all_buildin_nodes (cli_mode_t *cli_mode_p)
{
    cli_tree_node_t *exit_node_p, *end_node_p , *no_node_p, *default_node_p;

    if (!cli_mode_p->root_node_p) {
        return;
    }

    exit_node_p = clone_cli_node(&node_exit);
    if (!exit_node_p) {
        return;
    }
    end_node_p = clone_cli_node(&node_end);
    if (!end_node_p) {
        free(exit_node_p);
        return;
    }
    no_node_p = clone_cli_node(&node_no);
    if (!no_node_p) {
        free(exit_node_p);
        free(end_node_p);
        return;
    }
    default_node_p = clone_cli_node(&node_default);
    if (!default_node_p) {
        free(exit_node_p);
        free(end_node_p);
        free(no_node_p);
        return;
    }

    exit_node_p->acc_p = &node_exit_end;
    exit_node_p->alter_p = end_node_p;
    end_node_p->acc_p = &node_end_end;
    end_node_p->alter_p = no_node_p;
    no_node_p->acc_p = cli_mode_p->root_node_p;
    no_node_p->alter_p = default_node_p;
    default_node_p->acc_p = cli_mode_p->root_node_p;
    default_node_p->alter_p = cli_mode_p->root_node_p;

    cli_mode_p->root_node_p = exit_node_p;

    make_exit_help_str(cli_mode_p, exit_node_p);
    return;
}

static void
add_buildin_exit_node (cli_mode_t *cli_mode_p)
{
    cli_tree_node_t *exit_node_p, *end_node_p;

    exit_node_p = clone_cli_node(&node_exit);
    if (!exit_node_p) {
        return;
    }
    end_node_p = clone_cli_node(&node_end);
    if (!end_node_p) {
        free(exit_node_p);
        return;
    }

    exit_node_p->acc_p = &node_exit_end;
    exit_node_p->alter_p = end_node_p;
    end_node_p->acc_p = &node_end_end;
    end_node_p->alter_p = cli_mode_p->root_node_p;
    cli_mode_p->root_node_p = exit_node_p;

    make_exit_help_str(cli_mode_p, exit_node_p);
    return;
}

static void
add_buildin_nodes (cli_mode_t *cli_mode_p)
{
    if (cli_mode_p->mode == CLI_MODE_EXEC) {
        return;
    }

    if (!cli_mode_p->root_node_p) {
        add_buildin_exit_node(cli_mode_p);
        return;
    }

    if (is_cli_mode_in_config(cli_mode_p->mode)) {
        add_all_buildin_nodes(cli_mode_p);
    } else {
        add_buildin_exit_node(cli_mode_p);
    }

    return;
}

static void
add_buildin_nodes_to_cli_modes (void)
{
    uint32_t i;

    for (i = 0; i < cli_mode_cnt; i++) {
        add_buildin_nodes(&cli_mode_buf_p[i]);
    }
    return;
}

static void
change_num_node_keyword (cli_tree_node_t *node_p)
{
    char *str;

    if (!node_p || node_p->node_type == CLI_NODE_TYPE_DEAD) {
        return;
    }

    change_num_node_keyword(node_p->acc_p);
    change_num_node_keyword(node_p->alter_p);

    if (node_p->node_type != CLI_NODE_TYPE_NUMBER) {
        return;
    }

    str = calloc(NUM_NODE_HELP_STR_MAX_LEN+1, sizeof(char));
    if (!str) {
        return;
    }

    snprintf(str, NUM_NODE_HELP_STR_MAX_LEN+1,
             "<%d-%d>", node_p->min, node_p->max);
    node_p->keyword = str;
    return;
}

static void
change_all_num_node_keyword (void)
{
    uint32_t i;

    for (i = 0; i < cli_mode_cnt; i++) {
        change_num_node_keyword(cli_mode_buf_p[i].root_node_p);
    }
    return;
}

int
gvd_init_cli_tree (void)
{
    int rc;

    rc = init_cli_mode();
    if (rc == -1) {
        return -1;
    }

    rc = cli_link_default_root_nodes();
    if (rc == -1) {
        return -1;
    }

    rc = gvd_init_example_cli_tree();
    if (rc == -1) {
        return -1;
    }

    rc = gvd_init_example_cli_tree();

    change_all_num_node_keyword();

    // put as the last step
    add_buildin_nodes_to_cli_modes();

    gvd_tty_init_database();
    return 0;
}

