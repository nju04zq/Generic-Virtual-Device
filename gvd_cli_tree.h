/*
 * gvd_cli_tree.h
 *
 * By Qiang Zhou
 * October, 2014
 */

#ifndef __GVD_CLI_TREE_H__
#define __GVD_CLI_TREE_H__

#include <stdint.h>

#define link_name(root, mode) root ## mode

#define entry_name(node, mode) node ## mode

enum {
    CLI_NODE_TYPE_DEAD = 0,
    CLI_NODE_TYPE_IFELSE,
    CLI_NODE_TYPE_HELP,
    CLI_NODE_TYPE_END,
    CLI_NODE_TYPE_KEYWORD,
    CLI_NODE_TYPE_KEYWORD_ID,
    CLI_NODE_TYPE_NO,
    CLI_NODE_TYPE_DEFAULT,
    CLI_NODE_TYPE_NUMBER,
    CLI_NODE_TYPE_STRING,
    CLI_NODE_TYPE_WEEK_DAY,
    CLI_NODE_TYPE_TIME,
};

enum {
    CLI_MODE_NONE = 0,
    CLI_MODE_EXEC,
    CLI_MODE_SHELL,
    CLI_MODE_CONFIG,
    CLI_MODE_CONFIG_GVD,
};

struct cli_parser_info_s;
typedef int (*cli_handler)(struct cli_parser_info_s *);

typedef struct cli_tree_node_s {
    struct cli_tree_node_s *acc_p;
    struct cli_tree_node_s *alter_p;
	struct cli_tree_node_s *help_prev_p;
	struct cli_tree_node_s *help_next_p;
    char *keyword;
    cli_handler handler;
    int min;
    int max;
    int param1;
    int param2;
    char *help_string;
    int node_type;
    int submode_type;
    int flag;
} cli_tree_node_t;

typedef struct cli_tree_root_link_s {
    cli_tree_node_t *root_p;
    int cli_mode;
} cli_tree_root_link_t;

typedef struct cli_mode_def_s {
    int mode;
    int parent_mode;
    char *mode_string;
    char *help_string;
} cli_mode_def_t;

typedef struct cli_mode_s {
    int mode;
    int parent_mode;
    char *mode_string;
    char *help_string;
    struct cli_mode_s *parent_p;
    cli_tree_node_t *root_node_p;
} cli_mode_t;

int gvd_init_cli_tree(void);
cli_mode_t * gvd_find_cli_mode(int mode);
cli_mode_t *gvd_get_exec_cli_mode(void);
int cli_link_root_nodes(cli_tree_root_link_t **link_p, uint32_t cnt);
cli_tree_node_t *gvd_get_exec_cli_mode_root(void);
#endif //__GVD_CLI_TREE_H__
