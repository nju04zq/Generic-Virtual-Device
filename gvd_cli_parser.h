#ifndef __GVD_CLI_PARSER_H__
#define __GVD_CLI_PARSER_H__

#include <stdbool.h>
#include "gvd_common.h"
#include "gvd_cli_tree.h"

#define USER_DATA_MAX_LEN 127

#define OBJ(type, idx) (type<<8 | idx)

#define GET_OBJ(type, idx) cpi_p->type ## _buf[idx]

enum {
    PARSER_REQ_EXEC = 0,
    PARSER_REQ_QUERY,
    PARSER_REQ_AUTO_FILL,
};

enum {
    P_INVALID = -1,
    P_INT,
    P_STRING,
};

enum {
    P0 = 0,
    P1, P2, P3, P4, P5, P6,
    P7, P8, P9, P10,
    
    P_MAX,
};

struct gvd_tty_s;

typedef struct cli_parser_info_s {
    int flag;
    bool set_no;
    bool set_default;
    char user_data[USER_DATA_MAX_LEN+1];
    int P_INT_buf[P_MAX];
    char *P_STRING_buf[P_MAX];
    print_buffer_t cli_output;
    struct gvd_tty_s *tty_p;
    cli_tree_node_t *root_node_p;
    char *cli;
    char *cli_org;
} cli_parser_info_t;

int cli_parser_request(struct gvd_tty_s *tty_p, int req_code, char *cmd,
                       char **output);
uint32_t get_ps_len(void);
char *gvd_run_cli(struct gvd_tty_s *tty_p, char *cli);
#endif //__GVD_CLI_PARSER_H__
