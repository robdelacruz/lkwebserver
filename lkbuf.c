#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "lklib.h"

lkbuf_s *lkbuf_new(size_t bytes_size) {
    if (bytes_size == 0) {
        bytes_size = 10; //$$ specify a default initial size
    }

    lkbuf_s *buf = malloc(sizeof(lkbuf_s));
    buf->bytes_cur = 0;
    buf->bytes_len = 0;
    buf->bytes_size = bytes_size;
    buf->bytes = malloc(buf->bytes_size);
    return buf;
}

void lkbuf_free(lkbuf_s *buf) {
    assert(buf->bytes != NULL);
    free(buf->bytes);
    buf->bytes = NULL;
    free(buf);
}

int lkbuf_append(lkbuf_s *buf, char *bytes, size_t len) {
    // If not enough capacity to append bytes, expand the bytes buffer.
    if (len > buf->bytes_size - buf->bytes_len) {
        char *bs = realloc(buf->bytes, buf->bytes_size + len);
        if (bs == NULL) {
            return -1;
        }
        buf->bytes = bs;
        buf->bytes_size += len;
    }
    memcpy(buf->bytes + buf->bytes_len, bytes, len);
    buf->bytes_len += len;

    assert(buf->bytes != NULL);
    return 0;
}

#if 0
// Append to buf using sprintf() to a fixed length space.
// More efficient as no memory allocation needed, but it will
// truncate strings longer than BUF_LINE_MAXSIZE.
#define BUF_LINE_MAXSIZE 2048
void lkbuf_sprintf_line(lkbuf_s *buf, const char *fmt, ...) {
    char line[BUF_LINE_MAXSIZE];

    va_list args;
    va_start(args, fmt);
    int z = vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    // error in snprintf()
    if (z < 0) return;

    // if snprintf() truncated the output, fill in the terminating chars.
    if (z > sizeof(line)) {
        z = sizeof(line);
        line[z-2] = '\n';
        line[z-1] = '\0';
    }

    lkbuf_append(buf, line, strlen(line));
}
#endif

// Append to buf using asprintf().
// Can handle all string lengths without truncating, but less
// efficient as it allocs/deallocs memory.
void lkbuf_sprintf(lkbuf_s *buf, const char *fmt, ...) {
    int z;
    char *pstr = NULL;

    va_list args;
    va_start(args, fmt);
    z = vasprintf(&pstr, fmt, args);
    va_end(args);

    if (z == -1) return;

    lkbuf_append(buf, pstr, strlen(pstr)); //$$ use z+1 instead of strlen(pstr)?

    free(pstr);
    pstr = NULL;
}

