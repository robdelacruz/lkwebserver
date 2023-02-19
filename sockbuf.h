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
void sockbuf_free(sockbuf_t *sockbuf);

void sockbuf_expand_buf(sockbuf_t *sockbuf, size_t size);
void sockbuf_append(sockbuf_t *sockbuf, char *bytes, size_t bytes_size);
ssize_t sockbuf_read(sockbuf_t *sockbuf, char *dst, size_t count);
ssize_t sockbuf_readline(sockbuf_t *sb, char *dst, size_t dst_len);

void printbuf(char *buf, size_t buf_size);
void sockbuf_debugprint(sockbuf_t *sockbuf);

#endif

