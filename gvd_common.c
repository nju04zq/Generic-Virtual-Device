#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include "gvd_common.h"

#define PRINT_BUFFER_INIT_SIZE 1024
#define PRINT_BUFFER_GROW_SIZE 1024

#define SHELL_CMD_OUTPUT_BUF_SIZE 255

int
alloc_print_buffer (print_buffer_t *p)
{
    p->buf = calloc(PRINT_BUFFER_INIT_SIZE, sizeof(char));
    if (!p->buf) {
        return -1;
    }

    p->max_len = PRINT_BUFFER_INIT_SIZE;
    p->free_len = PRINT_BUFFER_INIT_SIZE;
    p->offset = 0;
    return 0;
}

static int
grow_print_buffer (print_buffer_t *p, uint32_t len)
{
    uint32_t grow_len, new_len;

    if (len < PRINT_BUFFER_INIT_SIZE) {
        grow_len = PRINT_BUFFER_GROW_SIZE;
    } else {
        grow_len = PRINT_BUFFER_GROW_SIZE + len;
    }

    new_len = p->max_len + grow_len;
    p->buf = realloc(p->buf, new_len);
    if (!p->buf) {
        return -1;
    }

    p->max_len = new_len;
    p->free_len += grow_len;
    return 0;
}

void
printb (print_buffer_t *p, const char *fmt, ...)
{
    va_list ap;
    uint32_t len = 0;
    char *buf_start;
    int rc;

    if (!p->buf) {
        return;
    }

    for (;;) {
        va_start(ap, fmt);
        buf_start = &p->buf[p->offset];
        len = vsnprintf(buf_start, p->free_len, fmt, ap);
        va_end(ap);

        if (len < p->free_len) {
            break;
        }

        rc = grow_print_buffer(p, len);
        if (rc == -1) {
            return;
        }
    }

    p->offset += len;
    p->free_len -= len;
    return;
}

void
free_print_buffer (print_buffer_t *p)
{
    if (p->buf) {
        free(p->buf);
    }
    return;
}

void
run_shell_cmd (char *cmd, print_buffer_t *output_p)
{
    FILE *read_fp;
    char buffer[SHELL_CMD_OUTPUT_BUF_SIZE+1];
    uint32_t chars_read, len;

    read_fp = popen(cmd, "r");
    if (!read_fp) {
        return;
    }

    for (;;) {
        memset(buffer, 0, sizeof(buffer));
        chars_read = fread(buffer, sizeof(char),
                           SHELL_CMD_OUTPUT_BUF_SIZE, read_fp);
        if (chars_read == 0) {
            break;
        }
        //TODO
        len = strlen(buffer);
        printb(output_p, buffer);
    }

    pclose(read_fp);
    return;
}

static int
get_file_size (FILE *fp)
{
    int cur_pos, file_size, result;

    cur_pos = ftell(fp);
    if (cur_pos == -1) {
        return -1;
    }

    result = fseek(fp, 0, SEEK_END);
    if (result != 0) {
        return -1;
    }

    file_size = ftell(fp);
    
    result = fseek(fp, cur_pos, SEEK_SET);
    if (result != 0) {
        return -1;
    }

    return file_size;
}

char *
read_file_content (char *file_name)
{
    FILE *fp;
    int file_size, read_size;
    char *file_content = NULL;

    fp = fopen(file_name, "r");
    if (!fp) {
        return NULL;
    }

    file_size = get_file_size(fp);
    if (file_size == -1) {
        fclose(fp);
        return NULL;
    }

    file_content = calloc(file_size+1, sizeof(char));
    if (!file_content) {
        fclose(fp);
        return NULL;
    }

    read_size = fread(file_content, sizeof(char), file_size, fp);
    if (read_size != file_size) {
        free(file_content);
        fclose(fp);
        return NULL;
    }

    fclose(fp);
    return file_content;
}

