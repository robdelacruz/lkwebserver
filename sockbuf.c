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
    memset(sb->buf, '.', sb->buf_size); // initialize for debugging purposes.

    return sb;
}

void sockbuf_free(sockbuf_t *sockbuf) {
    free(sockbuf->buf);
    sockbuf->buf = NULL;

    free(sockbuf);
}

// Read bytes from buffer, reading from socket as necessary to read additional bytes.
// This will block when reading from socket.
size_t sockbuf_read(sockbuf_t *sb, char *dst, size_t count) {
    assert(count > 0);
    assert(sb->buf_size >= sb->buf_len);
    assert(sb->buf_len >= sb->next_read_pos);

    if (sb->sockclosed) {
        return 0;
    }
    if (count == 0) {
        //$$ Better way to do this? As returning 0 signifies that socket is closed.
        errno = EINVAL;
        return 0;
    }

    int idst = 0;
    while (count > 0) {
        // If no buffer chars available, read from socket.
        if (sb->next_read_pos >= sb->buf_len) {
            memset(sb->buf, '.', sb->buf_size); // initialize for debugging purposes.
            int recvlen = recv(sb->sock, sb->buf, sb->buf_size, 0);
            if (recvlen == 0) {
                sb->sockclosed = 1;
                return idst;
            }
            sb->buf_len = recvlen;
            sb->next_read_pos = 0;
        }

        // Copy as much buffer chars as needed up to count.
        int num_bytes_to_copy;
        if (count > sb->buf_len - sb->next_read_pos) {
            num_bytes_to_copy = sb->buf_len - sb->next_read_pos;
        } else {
            num_bytes_to_copy = count;
        }
        memcpy(dst + idst, sb->buf + sb->next_read_pos, num_bytes_to_copy);

        idst += num_bytes_to_copy;
        count -= num_bytes_to_copy;
        sb->next_read_pos += num_bytes_to_copy;

        assert(count >= 0);
        assert(sb->next_read_pos <= sb->buf_len);
    }

//    printf("count: %ld\n", count);
//    printf("next_read_pos: %d\n", sb->next_read_pos);
//    printf("buf_len: %ld\n", sb->buf_len);

    assert(sb->buf_len >= sb->next_read_pos);

    return idst;
}

// Read one line from buffered socket, including the \n char, \0 terminated.
// This will block when reading from socket.
// Returns number of chars read or 0 for EOF.
size_t sockbuf_readline(sockbuf_t *sb, char *dst, size_t dst_len) {
    assert(dst_len > 2); // Reserve space for \n and \0.
    assert(sb->buf_size >= sb->buf_len);
    assert(sb->buf_len >= sb->next_read_pos);

    if (sb->sockclosed) {
        return 0;
    }
    if (dst_len <= 2) {
        //$$ Better way to do this? As returning 0 signifies that socket is closed.
        errno = EINVAL;
        return 0;
    }

    int idst = 0;
    while (1) {
        // If no buffer chars available, read from socket.
        if (sb->next_read_pos >= sb->buf_len) {
            memset(sb->buf, '.', sb->buf_size); // initialize for debugging purposes.
            int recvlen = recv(sb->sock, sb->buf, sb->buf_size, 0);
            if (recvlen == 0) {
                sb->sockclosed = 1;
                dst[idst] = '\0';
                return idst;
            }

            sb->buf_len = recvlen;
            sb->next_read_pos = 0;
        }

        // Copy unread buffer bytes into dst until a '\n' char.
        while (idst < dst_len-1) {
            if (sb->next_read_pos >= sb->buf_len) {
                break;
            }
            dst[idst] = sb->buf[sb->next_read_pos];
            sb->next_read_pos++;

            if (dst[idst] == '\n') {
                break;
            }
            idst++;
        }
        if (dst[idst] == '\n' || idst == dst_len-2) {
            break;
        }
    }

    assert(dst[idst] == '\n' || idst == dst_len-2);
    dst[idst+1] = '\0';
    return idst+1;
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


