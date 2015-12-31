#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define min(a,b) ((a)<(b)?(a):(b))

void
safe_strncpy (char *dst, const char *src, uint32_t len)
{
    if (!dst) return;
    
    if (!src) {
        dst[0]='\0';
        return;
    }

    strncpy(dst, src, len);
    dst[len] = '\0';
    return;
}

char *
safe_clone (const char *src, uint32_t len)
{
    char * buf;
    uint32_t needed_length;
    
    if (!src) {
        return NULL;
    }

    needed_length = strlen(src);
    if (len>0) needed_length = min(needed_length, len);
    
    buf = calloc(needed_length+1, sizeof(char));

    if (!buf) {
        return NULL;
    }
    strncpy(buf, src, needed_length);
    buf[needed_length] = '\0';
    return buf;
}

