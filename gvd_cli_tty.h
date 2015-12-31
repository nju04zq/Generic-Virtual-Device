/*
 * gvd_cli_tty.h
 *
 * By Qiang Zhou
 * October, 2014
 */
#ifndef __GVD_CLI_TTY_H__
#define __GVD_CLI_TTY_H__

#include <stdint.h>
#include <ncurses.h>

#define GVD_KEY_BACKSPACE   KEY_BACKSPACE
#define GVD_KEY_DEL         KEY_DC
#define GVD_KEY_ENTER       10 //'\n'
#define GVD_KEY_UP          KEY_UP
#define GVD_KEY_DOWN        KEY_DOWN
#define GVD_KEY_LEFT        KEY_LEFT
#define GVD_KEY_RIGHT       KEY_RIGHT
#define GVD_KEY_HOME        KEY_HOME
#define GVD_KEY_END         KEY_END
#define GVD_KEY_PGUP        KEY_PPAGE
#define GVD_KEY_PGDOWN      KEY_NPAGE
#define GVD_RESIZE          KEY_RESIZE
#define GVD_CTRL_C          3 //CTRL C
#define GVD_CTRL_Z          26 //CTRL Z

void tty_init(void);
void tty_reset(void);
bool tty_read_one_key(int *key_read);
void tty_move_cursor_left(void);
void tty_move_cursor_right(void);
void tty_move_cursor_pos(int pos);
void tty_get_scr_size(uint32_t *y_p, uint32_t *x_p);
#endif
