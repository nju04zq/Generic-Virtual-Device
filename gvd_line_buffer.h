#ifndef __GVD_LINE_BUFFER_H__
#define __GVD_LINE_BUFFER_H__

void printv(char *fmt, ...);
void replace_last_line(char *ps, char *cmd);
void scroll_up_refresh(void);
void scroll_down_refresh(void);
void resize_refresh(void);
void dump_all_lines_to_disk(void);
void clear_disk_logfile(void);
int line_buffer_init(void);
void line_buffer_clean(void);
#endif
