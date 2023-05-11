#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "lklib.h"

LKBuffer *lk_buffer_new(size_t bytes_size) {
    if (bytes_size == 0) {
        bytes_size = LK_BUFSIZE_SMALL;
    }

    LKBuffer *buf = lk_malloc(sizeof(LKBuffer), "lk_buffer_new");
    buf->bytes_cur = 0;
    buf->bytes_len = 0;
    buf->bytes_size = bytes_size;
    buf->bytes = lk_malloc(buf->bytes_size, "lk_buffer_new_bytes");
    return buf;
}

void lk_buffer_free(LKBuffer *buf) {
    assert(buf->bytes != NULL);
    lk_free(buf->bytes);
    buf->bytes = NULL;
    lk_free(buf);
}

void lk_buffer_resize(LKBuffer *buf, size_t bytes_size) {
    buf->bytes_size = bytes_size;
    if (buf->bytes_len > buf->bytes_size) {
        buf->bytes_len = buf->bytes_size;
    }
    if (buf->bytes_cur >= buf->bytes_len) {
        buf->bytes_cur = buf->bytes_len-1;
    }
    buf->bytes = lk_realloc(buf->bytes, buf->bytes_size, "lk_buffer_resize");
}

void lk_buffer_clear(LKBuffer *buf) {
    memset(buf->bytes, 0, buf->bytes_len);
    buf->bytes_len = 0;
    buf->bytes_cur = 0;
}

int lk_buffer_append(LKBuffer *buf, char *bytes, size_t len) {
    // If not enough capacity to append bytes, expand the bytes buffer.
    if (len > buf->bytes_size - buf->bytes_len) {
        char *bs = lk_realloc(buf->bytes, buf->bytes_size + len, "lk_buffer_append");
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

int lk_buffer_append_sz(LKBuffer *buf, char *s) {
    return lk_buffer_append(buf, s, strlen(s));
}

void lk_buffer_append_sprintf(LKBuffer *buf, const char *fmt, ...) {
    char sbuf[LK_BUFSIZE_MEDIUM];

    // Try to use static buffer first, to avoid allocation.
    va_list args;
    va_start(args, fmt);
    int z = vsnprintf(sbuf, sizeof(sbuf), fmt, args);
    if (z < 0) return;
    va_end(args);

    // If snprintf() truncated output to sbuf due to space,
    // use asprintf() instead.
    if (z >= sizeof(sbuf)) {
        va_list args;
        char *ps;
        va_start(args, fmt);
        int z = vasprintf(&ps, fmt, args);
        if (z == -1) return;
        va_end(args);

        lk_buffer_append(buf, ps, z);
        free(ps);
        return;
    }

    lk_buffer_append(buf, sbuf, z);
}

// Read CR-terminated line from buffer starting from buf->bytes_cur position.
// buf->bytes_cur is updated to point to the first char of next line.
// If dst doesn't have enough chars for line, partial line is copied.
// Returns number of bytes read.
size_t lk_buffer_readline(LKBuffer *buf, char *dst, size_t dst_len) {
    assert(dst_len > 2); // Reserve space for \n and \0.

    size_t nread = 0;
    while (nread < dst_len-1) { // leave space for null terminator
        if (buf->bytes_cur >= buf->bytes_len) {
            break;
        }
        dst[nread] = buf->bytes[buf->bytes_cur];
        nread++;
        buf->bytes_cur++;

        if (dst[nread-1] == '\n') {
            break;
        }
    }

    assert(nread <= dst_len-1);
    dst[nread] = '\0';
    return nread;
}

