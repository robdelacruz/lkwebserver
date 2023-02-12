#ifndef INPUTBUF_H
#define INPUTBUF_H

typedef struct {
    char *buf;
    size_t buf_size;
    size_t buf_len;
    unsigned int next_read_pos;
} inputbuf_t;

inputbuf_t *inputbuf_new(size_t initial_size);
void inputbuf_free(inputbuf_t *inputbuf);

void inputbuf_write(inputbuf_t *inputbuf, char *bytes, size_t bytes_size);
size_t inputbuf_read(inputbuf_t *inputbuf, char *dst, size_t count);

void printbuf(char *buf, size_t buf_size);
void inputbuf_debugprint(inputbuf_t *inputbuf);

#endif

