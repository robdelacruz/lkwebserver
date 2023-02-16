#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "sockbuf.h"

sockbuf_t *sockbuf_new(int sock, size_t initial_size) {
    sockbuf_t *sockbuf = malloc(sizeof(sockbuf_t));

//    if (initial_size == 0) {
//        initial_size = 1024;
//    }
    sockbuf->sock = sock;
    sockbuf->buf_size = initial_size;
    sockbuf->buf = malloc(initial_size);
    sockbuf->buf_len = 0;
    sockbuf->next_read_pos = 0;

    // Initialize unwritten buf space with '.' for debugging purposes.
    memset(sockbuf->buf, '.', sockbuf->buf_size);

    return sockbuf;
}

void sockbuf_free(sockbuf_t *sockbuf) {
    free(sockbuf->buf);
    sockbuf->buf = NULL;

    free(sockbuf);
}

// Expand memory allocated to buf by size.
void sockbuf_expand_buf(sockbuf_t *sockbuf, size_t size) {
    assert(sockbuf->buf_size >= sockbuf->buf_len);

    sockbuf->buf_size += size;
    sockbuf->buf = realloc(sockbuf->buf, sockbuf->buf_size);

    // Initialize unwritten buf space with '.' for debugging purposes.
    memset(sockbuf->buf + sockbuf->buf_len, '.',
           sockbuf->buf_size - sockbuf->buf_len);
}

void sockbuf_append(sockbuf_t *sockbuf, char *bytes, size_t bytes_size) {
    if (bytes == NULL || bytes_size == 0) {
        return;
    }

    assert(sockbuf->buf_size >= sockbuf->buf_len);

    // Make sure there's enough space in buf to append bytes.
    int num_available_bytes = sockbuf->buf_size - sockbuf->buf_len;
    if (bytes_size > num_available_bytes) {
        //$$ Consider exponentially growing buf rather than growing by bytes_size.
        sockbuf_expand_buf(sockbuf, num_available_bytes - bytes_size);
    }

    assert(sockbuf->buf_size - sockbuf->buf_len >= bytes_size);

    memcpy(sockbuf->buf + sockbuf->buf_len, bytes, bytes_size);
    sockbuf->buf_len += bytes_size;
}

// Read bytes from buffer, reading from socket as necessary to read additional bytes.
// This will block when reading from socket.
size_t sockbuf_read(sockbuf_t *sockbuf, char *dst, size_t count) {
    assert(sockbuf->buf_size >= sockbuf->buf_len);
    assert(sockbuf->buf_len >= sockbuf->next_read_pos);

    if (sockbuf->sockclosed) {
        return 0;
    }
    if (count == 0) {
        //$$ Find a better way to respond to this rather than returning 0
        // as returning 0 signifies that the socket is closed.
        errno = EINVAL;
        return 0;
    }

    // Check if there's enough bytes in buffer to receive the full count requested.
    // If not enough buffer bytes, recv additional bytes from the socket and append
    // if to buffer.
    int num_unread_bytes = sockbuf->buf_len - sockbuf->next_read_pos;
    if (count > num_unread_bytes) {
        int num_additional_bytes = count - num_unread_bytes;
        sockbuf_expand_buf(sockbuf, num_additional_bytes);

        int recvlen = recv(sockbuf->sock,
                           sockbuf->buf + sockbuf->next_read_pos,
                           num_additional_bytes,
                           0);
        if (recvlen == 0) {
            sockbuf->sockclosed = 1;
        }

        // Adjust count in case sock recv() received less bytes than requested.
        count -= num_additional_bytes - recvlen;

        sockbuf->buf_len += count;
    }

    assert(sockbuf->buf_len - sockbuf->next_read_pos >= count);

    memcpy(dst, sockbuf->buf + sockbuf->next_read_pos, count);
    sockbuf->next_read_pos += count;

//    printf("count: %ld\n", count);
//    printf("next_read_pos: %d\n", sockbuf->next_read_pos);
//    printf("buf_len: %ld\n", sockbuf->buf_len);

    assert(sockbuf->buf_len >= sockbuf->next_read_pos);

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

void sockbuf_debugprint(sockbuf_t *sockbuf) {
    printbuf(sockbuf->buf, sockbuf->buf_size);
    printf("buf_len: %ld\n", sockbuf->buf_len);
    printf("next_read_pos: %d\n", sockbuf->next_read_pos);
    printf("\n");
}


