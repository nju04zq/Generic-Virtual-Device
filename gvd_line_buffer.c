#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <ncurses.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "gvd_util.h"
#include "gvd_common.h"
#include "gvd_cli_tty.h"
#include "gvd_line_buffer.h"

//! line buffer struct size
#define LINE_BUFFER_SIZE 128
//! line buffer content max string length, 1 for '\0'
#define LINE_BUFFER_CONTENT_MAX_LEN \
    (LINE_BUFFER_SIZE - sizeof(struct line_buffer_s) - 1)
//! define for those extra long line, beyond this will be cut
#define BIG_LINE_BUFFER_SIZE 1024
//! big line buffer content max string length, 1 for '\0'
#define BIG_LINE_BUFFER_CONTENT_MAX_LEN \
    (BIG_LINE_BUFFER_SIZE - sizeof(struct line_buffer_s) - 1)
//! Line buffer number, will do disk flush when used up
#define LINE_BUFFER_NUM 1024
//! how many line buffers could one mask cover
#define LINE_BUFFER_MASK_SIZE (sizeof(int)*8)
//! bit mask, show line buffer availability
#define LINE_BUFFER_MASK_NUM (LINE_BUFFER_NUM/LINE_BUFFER_MASK_SIZE)
//! the temp output buffer size
#define OUTPUT_TEMP_SIZE 1023

#define FLAG_GET(flag, mask) ((flag) & (mask))
#define FLAG_SET(flag, mask) ((flag) |= (mask))
#define FLAG_UNSET(flag, mask) ((flag) &= (~mask))

//! whether this line buffer flushed to disk
#define LINE_BUFFER_FLAG_DIRTY (1 << 0)
//! whether this line buffer is a big one(needs free when useless)
#define LINE_BUFFER_FLAG_BIG   (1 << 1)

typedef struct line_buffer_s {
    struct line_buffer_s *prev;
    struct line_buffer_s *next;
    uint32_t len;
    uint8_t flag;
    char content[0];
} line_buffer_t;

typedef struct scr_dump_ctx_s {
    uint32_t scr_y;
    uint32_t scr_x;
    line_buffer_t *bottom_p;
    uint32_t bottom_offset;
} scr_dump_ctx_t;

void *line_buffers_memory = NULL;
static line_buffer_t* line_buffers[LINE_BUFFER_NUM];
static int line_buffer_mask[LINE_BUFFER_MASK_NUM];

static char output_temp[OUTPUT_TEMP_SIZE+1];

static line_buffer_t *head = NULL, *tail = NULL;

static char *console_log_name = "./.console_log";

static scr_dump_ctx_t scr_dump_ctx;

static int logfile_fd = -1;

void
clear_disk_logfile (void)
{
    if (logfile_fd == -1) {
        return;
    }

    ftruncate(logfile_fd, 0);
    lseek(logfile_fd, 0, SEEK_SET);
    return;
}

static void
dump_one_line_to_disk (line_buffer_t *p)
{
    if (logfile_fd == -1) {
        return;
    }

    write(logfile_fd, p->content, strlen(p->content));
    FLAG_UNSET(p->flag, LINE_BUFFER_FLAG_DIRTY);
    return;
}

void
dump_all_lines_to_disk (void)
{
    line_buffer_t *p = head;

    if (logfile_fd == -1) {
        return;
    }

    while (p != tail) {
        if (FLAG_GET(p->flag, LINE_BUFFER_FLAG_DIRTY)) {
            dump_one_line_to_disk(p);
        }
        p = p->next;
    }

    return;
}

static int
console_log_file_init (void)
{
    logfile_fd = open(console_log_name, O_CREAT|O_WRONLY|O_TRUNC, 0666);
    if (logfile_fd == -1) {
        return -1;
    }

    return 0;
}

static line_buffer_t *
decide_scr_dump_top (uint32_t *top_offset_p)
{
    line_buffer_t *bottom_p, *p;
    uint32_t bottom_offset, top_offset;
    uint32_t left_line, width, line_chars;

    bottom_p = scr_dump_ctx.bottom_p;
    bottom_offset = scr_dump_ctx.bottom_offset;
    left_line = scr_dump_ctx.scr_y;
    width = scr_dump_ctx.scr_x;

    line_chars = bottom_offset;
    p = bottom_p;
    while (left_line > 0 && p) {
        if (line_chars <= width) {
            p = p->prev;
            line_chars = p?p->len:0;
        } else {
            line_chars -= width;
        }

        left_line--;
    }

    if (!p) {
        p = head;
        top_offset = 0;
    } else if (line_chars == p->len) {
        p = p->next;
        top_offset = 0;
    } else {
        top_offset = ((line_chars+width-1)/width)*width;
    }

    *top_offset_p = top_offset;
    return p;
}

static void
dump_line_buffer_to_scr (line_buffer_t *top_p, uint32_t top_offset)
{
    line_buffer_t *bottom_p, *p;
    uint32_t bottom_offset;
    char temp;

    bottom_p = scr_dump_ctx.bottom_p;
    bottom_offset = scr_dump_ctx.bottom_offset;

    clear();

    p = top_p;
    printw("%s", &p->content[top_offset]);
    p = p->next;

    if (!p) {
        return;
    }

    while (p != bottom_p) {
        printw("%s", p->content);
        p = p->next;
    }

    if (bottom_offset == 0) {
        printw("%s", p->content);
    } else {
        temp = p->content[bottom_offset];
        p->content[bottom_offset] = '\0';
        printw("%s", p->content);
        p->content[bottom_offset] = temp;
    }

    return;
}

static void
decide_scroll_up_bottom (void)
{
    line_buffer_t *bottom_p, *p;
    uint32_t bottom_offset;
    uint32_t left_line, width, line_chars;

    bottom_p = scr_dump_ctx.bottom_p;
    bottom_offset = scr_dump_ctx.bottom_offset;
    left_line = scr_dump_ctx.scr_y/2;
    width = scr_dump_ctx.scr_x;

    line_chars = bottom_p->len - bottom_offset;
    p = bottom_p;
    while (left_line > 0 && p) {
        if (line_chars <= width) {
            p = p->prev;
            line_chars = p?p->len:0;
        } else {
            line_chars -= width;
        }

        left_line--;
    }

    if (!p) {
        p = head;
        bottom_offset = 0;
    } else if (line_chars == p->len) {
        p = p->next;
        bottom_offset = 0;
    } else {
        bottom_offset = ((line_chars+width-1)/width)*width;
    }

    scr_dump_ctx.bottom_offset = bottom_offset;
    scr_dump_ctx.bottom_p = p;

    return;
}

static void
re_calc_bottom_from_top (line_buffer_t *top_p)
{
    line_buffer_t *p;
    uint32_t bottom_offset;
    uint32_t left_line, width, line_chars;

    left_line = scr_dump_ctx.scr_y;
    width = scr_dump_ctx.scr_x;

    line_chars = top_p->len;
    p = top_p;
    while (left_line > 0 && p) {
        if (line_chars <= width) {
            p = p->next;
            line_chars = p?p->len:0;
        } else {
            line_chars -= width;
        }

        left_line--;
    }

    if (!p) {
        p = tail;
        bottom_offset = 0;
    } else if (p->len == line_chars) {
        p = p->prev;
        bottom_offset = 0;
    } else {
        bottom_offset = line_chars;
    }

    scr_dump_ctx.bottom_offset = bottom_offset;
    scr_dump_ctx.bottom_p = p;

    return;
}

void
scroll_up_refresh (void)
{
    line_buffer_t *top_p;
    uint32_t top_offset;

    decide_scroll_up_bottom();

    top_p = decide_scr_dump_top(&top_offset);

    if (top_p == head && top_offset == 0) {
        re_calc_bottom_from_top(top_p);
    }

    dump_line_buffer_to_scr(top_p, top_offset);

    return;
}

static void
decide_scroll_down_bottom (void)
{
    line_buffer_t *bottom_p, *p;
    uint32_t bottom_offset;
    uint32_t left_line, width, line_chars;

    bottom_p = scr_dump_ctx.bottom_p;
    bottom_offset = scr_dump_ctx.bottom_offset;
    left_line = scr_dump_ctx.scr_y/2;
    width = scr_dump_ctx.scr_x;

    line_chars = bottom_offset;
    p = bottom_p;
    while (left_line > 0 && p) {
        if (line_chars <= width) {
            p = p->next;
            line_chars = p?p->len:0;
        } else {
            line_chars -= width;
        }

        left_line--;
    }

    if (!p) {
        p = tail;
        bottom_offset = 0;
    } else if (line_chars == p->len) {
        p = p->prev;
        bottom_offset = 0;
    } else {
        bottom_offset = p->len - line_chars;
    }

    scr_dump_ctx.bottom_offset = bottom_offset;
    scr_dump_ctx.bottom_p = p;

    return;
}

void
scroll_down_refresh (void)
{
    line_buffer_t *top_p;
    uint32_t top_offset;

    decide_scroll_down_bottom();

    top_p = decide_scr_dump_top(&top_offset);

    dump_line_buffer_to_scr(top_p, top_offset);

    return;
}

static void
refresh_scr (void)
{
    line_buffer_t *top_p;
    uint32_t top_offset;

    scr_dump_ctx.bottom_p = tail;
    scr_dump_ctx.bottom_offset = tail->len;

    top_p = decide_scr_dump_top(&top_offset);

    dump_line_buffer_to_scr(top_p, top_offset);

    return;
}

void
resize_refresh (void)
{
    tty_get_scr_size(&scr_dump_ctx.scr_y, &scr_dump_ctx.scr_x);

    refresh_scr();
    return;
}

static void
mask_line_buffer (uint32_t idx, bool used)
{
    int mask;
    uint32_t bit_idx;

    mask = line_buffer_mask[idx/LINE_BUFFER_MASK_SIZE];
    bit_idx = idx & (~LINE_BUFFER_MASK_SIZE);

    if (used) {
        mask &= (~(1 << bit_idx));
    } else {
        mask |= (1 << bit_idx);
    }

    line_buffer_mask[idx/LINE_BUFFER_MASK_SIZE] = mask;
    return;
}

static void
clean_line_buffer (line_buffer_t *p)
{
    unsigned long idx;

    idx = ((unsigned long)p - (unsigned long)line_buffers_memory);
    idx /= LINE_BUFFER_SIZE;
    mask_line_buffer((uint32_t)idx, FALSE);
    memset(p, 0, LINE_BUFFER_SIZE);
    return;
}

static void
recycle_head_line (void)
{
    line_buffer_t *p;

    p = head;
    head = p->next;

    dump_one_line_to_disk(p);

    if (FLAG_GET(p->flag, LINE_BUFFER_FLAG_BIG)) {
        free(p);
    } else {
        clean_line_buffer(p);
    }

    return;
}

static void
recycle_line_buffer (void)
{
    uint32_t i, num;

    if (head == NULL || tail == NULL) {
        return;
    }

    num = LINE_BUFFER_NUM/4;
    for (i = 0; i < num; i++) {
        recycle_head_line();
    }
    return;
}

static int
get_avail_line_buffer_idx (void)
{
    int mask, idx;
    uint32_t i, pos, found;

    for (i = 0, pos = 0; i < LINE_BUFFER_MASK_NUM; i++) {
        mask = line_buffer_mask[i];
        found = ffs(mask);
        if (found > 0) {
            // -1 because found starts from 1
            idx = pos + found - 1;
            return idx;
        }

        pos += LINE_BUFFER_MASK_SIZE;
    }

    return -1;
}

static line_buffer_t *
get_avail_line_buffer (void)
{
    int idx;

    idx = get_avail_line_buffer_idx();
    if (idx >= LINE_BUFFER_NUM) {
        return NULL;
    } else if (idx >= 0) {
        mask_line_buffer(idx, TRUE);
        return line_buffers[idx];
    }

    if (idx == -1) {
        recycle_line_buffer();
    }

    idx = get_avail_line_buffer_idx();
    if (idx >= 0) {
        mask_line_buffer(idx, TRUE);
        return line_buffers[idx];
    }

    return NULL;
}

static int
switch_to_new_line_buffer (void)
{
    line_buffer_t *p;

    p = get_avail_line_buffer();
    if (p == NULL) {
        return -1;
    }

    FLAG_SET(p->flag, LINE_BUFFER_FLAG_DIRTY);

    scr_dump_ctx.bottom_offset = 0;
    scr_dump_ctx.bottom_p = p;

    // on init
    if (head == NULL || tail == NULL) {
        head = tail = p;
        return 0;
    }

    tail->next = p;
    p->prev = tail;
    tail = p;

    return 0;
}

static line_buffer_t *
prepare_big_line_buffer (line_buffer_t *p)
{
    line_buffer_t *p_big;

    p_big = calloc(1, BIG_LINE_BUFFER_SIZE);
    if (!p_big) {
        return NULL;
    }

    memcpy(p_big, p, LINE_BUFFER_SIZE);
    FLAG_SET(p_big->flag, LINE_BUFFER_FLAG_BIG);

    p_big->next = p->next;
    p_big->prev = p->prev;
    if (p_big->prev) {
        p_big->prev->next = p_big;
    } else {
        head = p_big;
    }
    if (p_big->next) {
        p_big->next->prev = p_big;
    } else {
        tail = p_big;
    }

    clean_line_buffer(p);

    return p_big;
}

static uint32_t
get_line_buffer_max_cp_len (line_buffer_t *p)
{
    uint32_t cp_len;

    if (FLAG_GET(p->flag, LINE_BUFFER_FLAG_BIG)) {
        cp_len = BIG_LINE_BUFFER_CONTENT_MAX_LEN - p->len;
    } else {
        cp_len = LINE_BUFFER_CONTENT_MAX_LEN - p->len;
    }
    return cp_len;
}

static void
save_one_line (line_buffer_t *p, char *str)
{
    char *start;
    uint32_t len, cp_len;

    len = strlen(str);
    cp_len = LINE_BUFFER_CONTENT_MAX_LEN;
    
    if (FLAG_GET(p->flag, LINE_BUFFER_FLAG_BIG) == TRUE &&
        (len + p->len) > BIG_LINE_BUFFER_CONTENT_MAX_LEN) {
        //tty_print_error("Line too long.\n");
        return;
    }

    if (FLAG_GET(p->flag, LINE_BUFFER_FLAG_BIG) == FALSE &&
        (len + p->len) > LINE_BUFFER_CONTENT_MAX_LEN) {
        p = prepare_big_line_buffer(p);
        if (!p) {
            return;
        }
    }

    cp_len = get_line_buffer_max_cp_len(p);
    cp_len = (len>cp_len)?cp_len:len;

    start = &p->content[p->len];
    safe_strncpy(start, str, cp_len);
    p->len = strlen(p->content);
    return;
}

static void
save_output_to_line_buffer (char *output)
{
    char *start, *end, *next;
    char temp = '\0';
    int rc;

    start = output;
    for (;;) {
        end = strchr(start, '\n');
        if (end) {
            next = end + 1;
            temp = *next;
            *next = '\0';
        } else {
            next = NULL;
        }

        save_one_line(tail, start);

        if (next) {
            *next = temp;
        } else {
            break;
        }

        rc = switch_to_new_line_buffer();
        if (rc == -1) {
            //tty_print_error("Run error on swtich to new line.\n");
            return;
        }
        start = next;
    }

    return;
}

static uint32_t
get_output_size (char *fmt, va_list ap)
{
    uint32_t written;

    written = vsnprintf(output_temp, OUTPUT_TEMP_SIZE+1, fmt, ap);
    return written;
}

static char *
print_to_temp (uint32_t output_size, char *fmt, va_list ap)
{
    char *output = output_temp;

    if (output_size > OUTPUT_TEMP_SIZE) {
        output = calloc(output_size+1, sizeof(char));
    }

    if (!output) {
        return NULL;
    }

    vsnprintf(output, output_size+1, fmt, ap);

    return output;
}

void
printv (char *fmt, ...)
{
    va_list ap;
    char *output;
    uint32_t output_size;

    va_start(ap, fmt);
    output_size = get_output_size(fmt, ap);
    va_end(ap);

    va_start(ap, fmt);
    output = print_to_temp(output_size, fmt, ap);
    va_end(ap);

    if (!output) {
        return;
    }

    save_output_to_line_buffer(output);

    if (output != output_temp) {
        free(output);
    }

    refresh_scr();

    return;
}

static void
replace_last_line_content (char *ps, char *cmd)
{
    line_buffer_t *p = tail;
    uint32_t len;

    len = strlen(ps) + strlen(cmd);

    if (FLAG_GET(p->flag, LINE_BUFFER_FLAG_BIG) == TRUE &&
        len > BIG_LINE_BUFFER_CONTENT_MAX_LEN) {
        //tty_print_error("Line too long.\n");
        return;
    }

    if (FLAG_GET(p->flag, LINE_BUFFER_FLAG_BIG) == FALSE &&
        len > LINE_BUFFER_CONTENT_MAX_LEN) {
        p = prepare_big_line_buffer(p);
        if (!p) {
            return;
        }
    }

    snprintf(p->content, len+1, "%s%s", ps, cmd);
    p->len = len;
    return;
}

void
replace_last_line (char *ps, char *cmd)
{
    replace_last_line_content(ps, cmd);
    refresh_scr();
    return;
}

static int
prepare_line_buffers (void)
{
    void *buf;
    uint32_t i;

    memset(line_buffer_mask, 0xff, sizeof(line_buffer_mask));

    buf = calloc(LINE_BUFFER_NUM, LINE_BUFFER_SIZE);
    if (!buf) {
        return -1;
    }
    line_buffers_memory = buf;

    for (i = 0; i < LINE_BUFFER_NUM; i++) {
        line_buffers[i] = (line_buffer_t *)buf;
        buf = (void *)((char *)buf + LINE_BUFFER_SIZE);
    }

    return 0;
}

void
line_buffer_clean (void)
{
    tty_reset();
    if (line_buffers_memory) {
        free(line_buffers_memory);
    }
    close(logfile_fd);
    return;
}

int
line_buffer_init (void)
{
    int rc;

    // make sure there aren't used mask
    if (LINE_BUFFER_NUM & (LINE_BUFFER_MASK_SIZE-1)) {
        return -1;
    }

    tty_init();

    tty_get_scr_size(&scr_dump_ctx.scr_y, &scr_dump_ctx.scr_x);

    rc = prepare_line_buffers();
    if (rc == -1) {
        line_buffer_clean();
        return -1;
    }

    rc = console_log_file_init();
    if (rc == -1) {
        line_buffer_clean();
        return -1;
    }

    rc = switch_to_new_line_buffer();
    if (rc == -1) {
        line_buffer_clean();
        return -1;
    }

    return 0;
}

