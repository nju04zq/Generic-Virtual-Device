#ifndef __GVD_COMMON_H__
#define __GVD_COMMON_H__

#include <stdint.h>

typedef struct print_buffer_s {
    int cmd_ret;
    char *buf;
    uint32_t max_len;
    uint32_t offset;
    uint32_t free_len;
} print_buffer_t;

int
alloc_print_buffer(print_buffer_t *p);

void
printb(print_buffer_t *p, const char *fmt, ...);

void
free_print_buffer(print_buffer_t *p);

void
run_shell_cmd(char *cmd, print_buffer_t *output_p);

char *
read_file_content(char *file_name);
#endif //__GVD_COMMON_H__
