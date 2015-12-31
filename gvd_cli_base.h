/*
 * gvd_cli_base.h
 *
 * By Qiang Zhou
 * October, 2014
 */

#ifndef __GVD_CLI_BASE_H__
#define __GVD_CLI_BASE_H__

#include <stddef.h>
#include "gvd_cli_tree.h"

extern cli_tree_node_t node_dead;

#define NO_ALT node_dead

#define DEF_NO(node_name, node_acc, node_alter, keyword, help_string) \
static cli_tree_node_t node_name = {&node_acc, &node_alter, NULL, NULL,\
                                    keyword, NULL, 0, 0, -1, -1, help_string,\
                                    CLI_NODE_TYPE_NO, CLI_MODE_NONE, 0};

#define DEF_DEFAULT(node_name, node_acc, node_alter, keyword, help_string) \
static cli_tree_node_t node_name = {&node_acc, &node_alter, NULL, NULL,\
                                    keyword, NULL, 0, 0, -1, -1, help_string,\
                                    CLI_NODE_TYPE_DEFAULT, CLI_MODE_NONE, 0};

#define KEYWORD(node_name, node_acc, node_alter, keyword, help_string) \
static cli_tree_node_t node_name = {&node_acc, &node_alter, NULL, NULL,\
                                    keyword, NULL, 0, 0, -1, -1, help_string,\
                                    CLI_NODE_TYPE_KEYWORD, CLI_MODE_NONE, 0};

#define KEYWORD_ID(node_name, node_acc, node_alter, param, flag, keyword, \
                   help_string) \
static cli_tree_node_t node_name = {&node_acc, &node_alter, NULL, NULL,\
                                    keyword, NULL, 0, 0, param, -1,\
                                    help_string, CLI_NODE_TYPE_KEYWORD_ID,\
                                    CLI_MODE_NONE, flag};

#define STRING_MAX(node_name, node_acc, node_alter, param, max, help_string) \
static cli_tree_node_t node_name = {&node_acc, &node_alter, NULL, NULL,\
                                    "WORD", NULL, 0, max, param, -1,\
                                    help_string, CLI_NODE_TYPE_STRING,\
                                    CLI_MODE_NONE, 0};

#define STRING(node_name, node_acc, node_alter, param, help_string) \
        STRING_MAX(node_name, node_acc, node_alter, param, 0, help_string)    

#define NUMBER(node_name, node_acc, node_alter, param, min, max, help_string) \
static cli_tree_node_t node_name = {&node_acc, &node_alter, NULL, NULL,\
                                    "NUMBER", NULL, min, max, param, -1,\
                                    help_string,\
                                    CLI_NODE_TYPE_NUMBER, CLI_MODE_NONE, 0};

#define END_FLAG_SUBMODE(node_name, handler, submode, flag) \
static cli_tree_node_t node_name = {NULL, NULL, NULL, NULL,\
                                    "<cr>", handler, 0, 0, -1, -1, "",\
                                    CLI_NODE_TYPE_END, submode, flag};

#define END_FLAG(node_name, handler, flag) \
        END_FLAG_SUBMODE(node_name, handler, CLI_MODE_NONE, flag)

#define END_SUBMODE(node_name, handler, submode) \
        END_FLAG_SUBMODE(node_name, handler, submode, 0)

#define END(node_name, handler) \
        END_FLAG_SUBMODE(node_name, handler, CLI_MODE_NONE, 0)

#define IFELSE(node_name, node_acc, node_alter, condition) \
static int node_name##_func (struct cli_parser_info_s *cpi_p) \
{ return (condition); }\
static cli_tree_node_t node_name = {&node_acc, &node_alter, NULL, NULL,\
                                    "", node_name##_func, 0, 0, -1, -1,  "",\
                                    CLI_NODE_TYPE_IFELSE, CLI_MODE_NONE, 0};

#define HELP(node_name, node_acc, help_handler, help_string) \
static cli_tree_node_t node_name = {&node_acc, NULL, NULL, NULL,\
                                    "", help_handler, 0, 0, -1, -1,\
                                    help_string,\
                                    CLI_NODE_TYPE_HELP, CLI_MODE_NONE, 0};

#define WEEK_DAY(node_name, node_acc, node_alter, param, help_string) \
static cli_tree_node_t node_name = {&node_acc, &node_alter, NULL, NULL,\
                                    "DAY", NULL, 0, 0, param, -1, help_string,\
                                    CLI_NODE_TYPE_WEEK_DAY, CLI_MODE_NONE, 0};

#define TIME(node_name, node_acc, node_alter, param1, param2, help_string) \
static cli_tree_node_t node_name = {&node_acc, &node_alter, NULL, NULL,\
                                    "hh:mm", NULL, 0, 0, param1, param2,\
                                    help_string, CLI_NODE_TYPE_TIME,\
                                    CLI_MODE_NONE, 0};

#define LINK_ROOT(node_name, mode) \
static cli_tree_root_link_t link_name(node_name, mode) = {&node_name, mode};
#endif //__GVD_CLI_BASE_H__
