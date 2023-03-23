#ifndef NETFUNCS_H
#define NETFUNCS_H

#include "lklib.h"

/*** httprequest_s - HTTP Request struct ***/
typedef struct {
    lkstr_s *method;       // GET
    lkstr_s *uri;          // /path/to/index.html
    lkstr_s *version;      // HTTP/1.0
    lkstringmap_s *headers;
    lkbuf_s *head;
    lkbuf_s *body;
} httprequest_s;

httprequest_s *httprequest_new();
void httprequest_free(httprequest_s *req);
void httprequest_parse_request_line(httprequest_s *req, char *line);
void httprequest_parse_header_line(httprequest_s *req, char *line);
void httprequest_add_header(httprequest_s *req, char *k, char *v);
void httprequest_append_body(httprequest_s *req, char *bytes, int bytes_len);
void httprequest_debugprint(httprequest_s *req);


/*** httpresponse_s - HTTP Response struct ***/
typedef struct {
    int status;             // 404
    lkstr_s *statustext;    // File not found
    lkstr_s *version;       // HTTP/1.0
    lkstringmap_s *headers;
    lkbuf_s *head;
    lkbuf_s *body;
} httpresponse_s;

httpresponse_s *httpresponse_new();
void httpresponse_free(httpresponse_s *resp);
void httpresponse_add_header(httpresponse_s *resp, char *k, char *v);
void httpresponse_gen_headbuf(httpresponse_s *resp);
void httpresponse_debugprint(httpresponse_s *resp);


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

