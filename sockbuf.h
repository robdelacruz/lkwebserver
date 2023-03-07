#ifndef SOCKBUF_H
#define SOCKBUF_H

typedef struct {
    int sock;
    char *buf;
    size_t buf_size;
    size_t buf_len;
    unsigned int next_read_pos;
    int sockclosed;
} sockbuf_t;

sockbuf_t *sockbuf_new(int sock, size_t initial_size);
void sockbuf_free(sockbuf_t *sb);

int sockbuf_eof(sockbuf_t *sb);
ssize_t sockbuf_readline(sockbuf_t *sb, char *dst, size_t dst_len);

ssize_t recvn(int sock, char *buf, size_t count);
void set_sock_timeout(int sock, int nsecs, int ms);
void set_sock_nonblocking(int sock);

#endif

