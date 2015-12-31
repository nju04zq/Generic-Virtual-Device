/*
 * gvd_cli_tty.c
 *
 * By Qiang Zhou
 * October, 2014
 */

#include <ncurses.h>
#include <string.h>
#include "gvd_cli_tty.h"

void
tty_init (void)
{
    initscr();
    //cbreak();
    raw();
    //Enter key is \n in nl(), \r in nonl()
    nl();
    scrollok(stdscr, FALSE);
    noecho();
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);
    refresh();
}

void
tty_reset (void)
{
    endwin();
}

bool
tty_read_one_key (int *key_read)
{
    *key_read = getch();
    return TRUE;
}

void
tty_move_cursor_left (void)
{
    int x, y;

    getyx(stdscr, y, x);
    move(y, --x);

    return;
}

void
tty_move_cursor_right (void)
{
    int x, y;

    getyx(stdscr, y, x);
    move(y, ++x);

    return;
}

//cursor pos (x,y) starts from (0,0)
void
tty_move_cursor_pos (int pos)
{
    int x, y;

    getyx(stdscr, y, x);
    move(y, pos);

    return;
}

void
tty_get_scr_size (uint32_t *y_p, uint32_t *x_p)
{
    getmaxyx(stdscr, *y_p, *x_p);
    return;
}

