#ifndef NETFUNCS_H
#define NETFUNCS_H

#include "lklib.h"

/*** httpreq_t - HTTP Request struct ***/
typedef struct {
    lkstr_s *method;       // GET
    lkstr_s *uri;          // /path/to/index.html
    lkstr_s *version;      // HTTP/1.0
    lkstringmap_s *headers;
    lkbuf_s *head;
    lkbuf_s *body;
} httpreq_t;

httpreq_t *httpreq_new();
void httpreq_free(httpreq_t *req);
void httpreq_parse_request_line(httpreq_t *req, char *line);
void httpreq_parse_header_line(httpreq_t *req, char *line);
void httpreq_add_header(httpreq_t *req, char *k, char *v);
void httpreq_append_body(httpreq_t *req, char *bytes, int bytes_len);
void httpreq_debugprint(httpreq_t *req);

/*** httpresp_t - HTTP Response struct ***/
typedef struct {
    int status;             // 404
    lkstr_s *statustext;    // File not found
    lkstr_s *version;       // HTTP/1.0
    lkstringmap_s *headers;
    lkbuf_s *head;
    lkbuf_s *body;
} httpresp_t;

httpresp_t *httpresp_new();
void httpresp_free(httpresp_t *resp);
void httpresp_add_header(httpresp_t *resp, char *k, char *v);
void httpresp_gen_headbuf(httpresp_t *resp);
void httpresp_debugprint(httpresp_t *resp);


/*** sockbuf_t - Buffered input for sockets ***/
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
ssize_t sockbuf_readline(sockbuf_t *sb, char *dst, size_t dst_len);
void sockbuf_debugprint(sockbuf_t *sb);
void debugprint_buf(char *buf, size_t buf_size);


/*** socket helper functions ***/
ssize_t sock_recv(int sock, char *buf, size_t count);
ssize_t sock_send(int sock, char *buf, size_t count);
void set_sock_timeout(int sock, int nsecs, int ms);
void set_sock_nonblocking(int sock);

/*** Other helper functions ***/
// Remove trailing CRLF or LF (\n) from string.
void chomp(char* s);
// Read entire file into buf.
ssize_t readfile(char *filepath, lkbuf_s *buf);
// Read entire file descriptor contents into buf.
ssize_t readfd(int fd, lkbuf_s *buf);
// Append src to dest, allocating new memory in dest if needed.
// Return new pointer to dest.
char *astrncat(char *dest, char *src, size_t src_len);

#endif

