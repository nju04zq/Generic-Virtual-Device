#ifndef __GVD_UTIL_H__
#define __GVD_UTIL_H__

#include <stdint.h>

#ifndef TRUE
    #define TRUE true
#endif

#ifndef FALSE
    #define FALSE false
#endif

#define CMD_MAX_LEN 255

#define ARRAY_LEN(a) (sizeof(a)/sizeof(a[0]))

enum {
    PROCESS_CONTINUE = 0,
    PROCESS_SUBMODE_ERROR,
    PROCESS_EXIT,
};

void
safe_strncpy(char *dst, const char *src, uint32_t len);

char *
safe_clone(const char *src, uint32_t len);
#endif //__GVD_UTIL_H__
