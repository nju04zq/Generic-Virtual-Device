#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "gvd_tty.h"
#include "gvd_util.h"

typedef struct tty_ctrl_s {
    struct tty_ctrl_s *next;
    uint32_t tty_id;
    gvd_tty_t tty;
} tty_ctrl_t;

static pthread_mutex_t tty_ctrl_mutex;
static tty_ctrl_t *tty_ctrl_head = NULL;
static uint32_t next_tty_id = 1000;

extern gvd_tty_t gvd_tty;

static void
insert_tty_ctrl (tty_ctrl_t *tty_ctrl_p)
{
    tty_ctrl_p->next = tty_ctrl_head;
    tty_ctrl_head = tty_ctrl_p;
    return;
}

static void
init_tty (gvd_tty_t *tty_p)
{
    memset(&tty_p->cpi, 0, sizeof(cli_parser_info_t));
    tty_p->cli_mode_p = gvd_get_exec_cli_mode();
    tty_p->cpi.tty_p = tty_p;
    return;
}

static uint32_t
create_tty (void)
{
    tty_ctrl_t *tty_ctrl_p;

    tty_ctrl_p = calloc(1, sizeof(tty_ctrl_t));
    if (!tty_ctrl_p) {
        return GVD_INVALID_VTY_ID;
    }

    tty_ctrl_p->tty_id = (next_tty_id++);
    init_tty(&tty_ctrl_p->tty);

    insert_tty_ctrl(tty_ctrl_p);

    return tty_ctrl_p->tty_id;
}

uint32_t
gvd_create_tty (void)
{
    uint32_t tty_id;

    pthread_mutex_lock(&tty_ctrl_mutex);
    tty_id = create_tty();
    pthread_mutex_unlock(&tty_ctrl_mutex);
    return tty_id;
}

static void
remove_tty_ctrl (uint32_t tty_id)
{
    tty_ctrl_t *prev_tty_ctrl_p, *cur_tty_ctrl_p;

    prev_tty_ctrl_p = NULL;
    cur_tty_ctrl_p = tty_ctrl_head;
    while (cur_tty_ctrl_p) {
        if (cur_tty_ctrl_p->tty_id == tty_id) {
            break;
        }
        prev_tty_ctrl_p = cur_tty_ctrl_p;
        cur_tty_ctrl_p = cur_tty_ctrl_p->next;
    }

    if (!cur_tty_ctrl_p) {
        return;
    }

    if (!prev_tty_ctrl_p) {
        tty_ctrl_head = cur_tty_ctrl_p->next;
    } else {
        prev_tty_ctrl_p->next = cur_tty_ctrl_p->next;
    }

    free(cur_tty_ctrl_p);
    return;
}

void
gvd_destory_tty (uint32_t tty_id)
{
    pthread_mutex_lock(&tty_ctrl_mutex);
    remove_tty_ctrl(tty_id);
    pthread_mutex_unlock(&tty_ctrl_mutex);
    return;
}

static tty_ctrl_t *
find_tty_ctrl (uint32_t tty_id)
{
    tty_ctrl_t *tty_ctrl_p;

    tty_ctrl_p = tty_ctrl_head;
    while (tty_ctrl_p) {
        if (tty_ctrl_p->tty_id == tty_id) {
            return tty_ctrl_p;
        }
        tty_ctrl_p = tty_ctrl_p->next;
    }

    return NULL;
}

static char *
tty_run_cli (uint32_t tty_id, char *cli)
{
    tty_ctrl_t *tty_ctrl_p;
    char *output;

    tty_ctrl_p = find_tty_ctrl(tty_id);
    if (!tty_ctrl_p) {
        output = safe_clone("Invalid VTY to run CLI.\n\n", 0);
        return output;
    }

    output = gvd_run_cli(&tty_ctrl_p->tty, cli);
    return output;
}

char *
gvd_tty_run_cli (uint32_t tty_id, char *cli)
{
    char *output;

    pthread_mutex_lock(&tty_ctrl_mutex);
    output = tty_run_cli(tty_id, cli);
    pthread_mutex_unlock(&tty_ctrl_mutex);
    return output;
}

void
gvd_enter_lower_cli_mode (gvd_tty_t *tty_p, int mode)
{
    cli_mode_t *cli_mode_p;

    cli_mode_p = gvd_find_cli_mode(mode);
    if (!cli_mode_p) {
        return;
    }

    tty_p->cli_mode_p = cli_mode_p;
    return;
}

void
gvd_return_upper_cli_mode (gvd_tty_t *tty_p)
{
    if (tty_p->cli_mode_p->parent_p) {
        tty_p->cli_mode_p = tty_p->cli_mode_p->parent_p;
    }
    return;
}

void
gvd_return_exec_cli_mode (gvd_tty_t *tty_p)
{
    tty_p->cli_mode_p = gvd_get_exec_cli_mode();
    return;
}

void
gvd_tty_init_database (void)
{
    init_tty(&gvd_tty);
    (void)pthread_mutex_init(&tty_ctrl_mutex, NULL);
    return;
}

