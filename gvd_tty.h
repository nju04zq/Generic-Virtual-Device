#ifndef __GVD_TTY_H__
#define __GVD_TTY_H__

#include <stdint.h>
#include "gvd_cli_tree.h"
#include "gvd_cli_parser.h"

#define GVD_INVALID_VTY_ID 0

typedef struct gvd_tty_s {
    cli_mode_t *cli_mode_p;
    cli_parser_info_t cpi;
} gvd_tty_t;

void
gvd_enter_lower_cli_mode(gvd_tty_t *tty_p, int mode);

void
gvd_return_upper_cli_mode(gvd_tty_t *tty_p);

void
gvd_return_exec_cli_mode(gvd_tty_t *tty_p);

uint32_t
gvd_create_tty(void);

void
gvd_destory_tty(uint32_t tty_id);

char *
gvd_tty_run_cli(uint32_t tty_id, char *cli);

void
gvd_tty_init_database(void);
#endif //__GVD_TTY_H__
