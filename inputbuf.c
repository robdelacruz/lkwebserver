#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "inputbuf.h"

inputbuf_t *inputbuf_new(size_t initial_size) {
    inputbuf_t *inputbuf = malloc(sizeof(inputbuf_t));

//    if (initial_size == 0) {
//        initial_size = 1024;
//    }
    inputbuf->buf = malloc(initial_size);
    inputbuf->buf_size = initial_size;

    // Initialize unwritten buf space with '.' for debugging purposes.
    memset(inputbuf->buf, '.', inputbuf->buf_size);

    inputbuf->buf_len = 0;
    inputbuf->next_read_pos = 0;

    return inputbuf;
}

void inputbuf_free(inputbuf_t *inputbuf) {
    free(inputbuf->buf);
    inputbuf->buf = NULL;

    free(inputbuf);
}

void inputbuf_write(inputbuf_t *inputbuf, char *bytes, size_t bytes_size) {
    if (bytes == NULL || bytes_size == 0) {
        return;
    }

    // Make sure there's enough space in buf to append bytes.
    if (inputbuf->buf_len + bytes_size > inputbuf->buf_size) {
        int nbytes = inputbuf->buf_len + bytes_size - inputbuf->buf_size;
        inputbuf->buf_size += nbytes;
        inputbuf->buf = realloc(inputbuf->buf, inputbuf->buf_size);

        // Initialize unwritten buf space with '.' for debugging purposes.
        memset(inputbuf->buf + inputbuf->buf_len, '.',
               inputbuf->buf_size - inputbuf->buf_len);
    }

    memcpy(inputbuf->buf + inputbuf->buf_len, bytes, bytes_size);
    inputbuf->buf_len += bytes_size;
}

size_t inputbuf_read(inputbuf_t *inputbuf, char *dst, size_t count) {
    if (inputbuf->buf_len == 0 || count == 0) {
        return 0;
    }
    if (inputbuf->next_read_pos > inputbuf->buf_len-1) {
        return 0;
    }

    if (inputbuf->next_read_pos + count > inputbuf->buf_len) {
        count = inputbuf->buf_len - inputbuf->next_read_pos;
    }

    memcpy(dst, inputbuf->buf + inputbuf->next_read_pos, count);
    inputbuf->next_read_pos += count;

//    printf("count: %ld\n", count);
//    printf("next_read_pos: %d\n", inputbuf->next_read_pos);

    return count;
}

void printbuf(char *buf, size_t buf_size) {
    printf("buf: ");
    for (int i=0; i < buf_size; i++) {
        putchar(buf[i]);
    }
    putchar('\n');
    printf("buf_size: %ld\n", buf_size);
}

void inputbuf_debugprint(inputbuf_t *inputbuf) {
    printbuf(inputbuf->buf, inputbuf->buf_size);
    printf("buf_len: %ld\n", inputbuf->buf_len);
    printf("next_read_pos: %d\n", inputbuf->next_read_pos);
    printf("\n");
}


