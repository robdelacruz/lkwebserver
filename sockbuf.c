#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "sockbuf.h"

sockbuf_t *sockbuf_new(int sock, size_t buf_size) {
    sockbuf_t *sb = malloc(sizeof(sockbuf_t));

    if (buf_size == 0) {
        buf_size = 1024;
    }
    sb->sock = sock;
    sb->buf_size = buf_size;
    sb->buf = malloc(buf_size);
    sb->buf_len = 0;
    sb->next_read_pos = 0;
    memset(sb->buf, '*', sb->buf_size); // initialize for debugging purposes.

    return sb;
}

void sockbuf_free(sockbuf_t *sockbuf) {
    free(sockbuf->buf);
    sockbuf->buf = NULL;

    free(sockbuf);
}

// Read bytes from buffer, reading from socket as necessary to read additional bytes.
// This will block when reading from socket.
ssize_t sockbuf_read(sockbuf_t *sb, char *dst, size_t count) {
    assert(count > 0);
    assert(sb->buf_size >= sb->buf_len);
    assert(sb->buf_len >= sb->next_read_pos);

    if (sb->sockclosed) {
        return 0;
    }
    if (count == 0) {
        errno = EINVAL;
        return -1;
    }

    int nbytes_read = 0;
    while (count > 0) {
        // If no buffer chars available, read from socket.
        if (sb->next_read_pos >= sb->buf_len) {
            memset(sb->buf, '*', sb->buf_size); // initialize for debugging purposes.
            int recvlen = recv(sb->sock, sb->buf, sb->buf_size, 0);
            if (recvlen == 0) {
                sb->sockclosed = 1;
                break;
            }
            sb->buf_len = recvlen;
            sb->next_read_pos = 0;
        }

        // Copy as much buffer chars as needed up to count.
        int nbufbytes;
        if (count > sb->buf_len - sb->next_read_pos) {
            nbufbytes = sb->buf_len - sb->next_read_pos;
        } else {
            nbufbytes = count;
        }
        memcpy(dst + nbytes_read, sb->buf + sb->next_read_pos, nbufbytes);

        nbytes_read += nbufbytes;
        count -= nbufbytes;
        sb->next_read_pos += nbufbytes;

        assert(count >= 0);
        assert(sb->buf_len >= sb->next_read_pos);
    }

    assert(sb->buf_len >= sb->next_read_pos);

//    printf("count: %ld\n", count);
//    printf("next_read_pos: %d\n", sb->next_read_pos);
//    printf("buf_len: %ld\n", sb->buf_len);

    return nbytes_read;
}

// Read one line from buffered socket, including the \n char, \0 terminated.
// This will block when reading from socket.
// Returns number of chars read or 0 for EOF.
ssize_t sockbuf_readline(sockbuf_t *sb, char *dst, size_t dst_len) {
    assert(dst_len > 2); // Reserve space for \n and \0.
    assert(sb->buf_size >= sb->buf_len);
    assert(sb->buf_len >= sb->next_read_pos);

    if (sb->sockclosed) {
        return 0;
    }
    if (dst_len <= 2) {
        errno = EINVAL;
        return -1;
    }

    int nbytes_read = 0;

    // Leave space for string null terminator.
    while (nbytes_read < dst_len-1) {
        // If no buffer chars available, read from socket.
        if (sb->next_read_pos >= sb->buf_len) {
            memset(sb->buf, '*', sb->buf_size); // initialize for debugging purposes.
            int recvlen = recv(sb->sock, sb->buf, sb->buf_size, 0);
            if (recvlen == 0) {
                sb->sockclosed = 1;
                break;
            }
            sb->buf_len = recvlen;
            sb->next_read_pos = 0;
        }

        // Copy unread buffer bytes into dst until a '\n' char.
        while (nbytes_read < dst_len-1) {
            if (sb->next_read_pos >= sb->buf_len) {
                break;
            }
            dst[nbytes_read] = sb->buf[sb->next_read_pos];
            nbytes_read++;
            sb->next_read_pos++;

            if (dst[nbytes_read-1] == '\n') {
                goto readline_end;
            }
        }
    }

readline_end:
    assert(nbytes_read <= dst_len-1);
    dst[nbytes_read] = '\0';
    return nbytes_read;
}

void printbuf(char *buf, size_t buf_size) {
    printf("buf: ");
    for (int i=0; i < buf_size; i++) {
        putchar(buf[i]);
    }
    putchar('\n');
    printf("buf_size: %ld\n", buf_size);
}

void sockbuf_debugprint(sockbuf_t *sb) {
    printf("buf_size: %ld\n", sb->buf_size);
    printf("buf_len: %ld\n", sb->buf_len);
    printf("next_read_pos: %d\n", sb->next_read_pos);
    printbuf(sb->buf, sb->buf_size);
    printf("\n");
}


