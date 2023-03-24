#ifndef NETFUNCS_H
#define NETFUNCS_H

#include "lklib.h"

/*** lkhttprequest_s - HTTP Request struct ***/
typedef struct {
    lkstr_s *method;       // GET
    lkstr_s *uri;          // /path/to/index.html
    lkstr_s *version;      // HTTP/1.0
    lkstringmap_s *headers;
    lkbuf_s *head;
    lkbuf_s *body;
} lkhttprequest_s;

lkhttprequest_s *lkhttprequest_new();
void lkhttprequest_free(lkhttprequest_s *req);
void lkhttprequest_add_header(lkhttprequest_s *req, char *k, char *v);
void lkhttprequest_append_body(lkhttprequest_s *req, char *bytes, int bytes_len);
void lkhttprequest_debugprint(lkhttprequest_s *req);


/*** lkhttpresponse_s - HTTP Response struct ***/
typedef struct {
    int status;             // 404
    lkstr_s *statustext;    // File not found
    lkstr_s *version;       // HTTP/1.0
    lkstringmap_s *headers;
    lkbuf_s *head;
    lkbuf_s *body;
} lkhttpresponse_s;

lkhttpresponse_s *lkhttpresponse_new();
void lkhttpresponse_free(lkhttpresponse_s *resp);
void lkhttpresponse_add_header(lkhttpresponse_s *resp, char *k, char *v);
void lkhttpresponse_gen_headbuf(lkhttpresponse_s *resp);
void lkhttpresponse_debugprint(lkhttpresponse_s *resp);


/*** lksocketreader_s - Buffered input for sockets ***/
typedef struct {
    int sock;
    char *buf;
    size_t buf_size;
    size_t buf_len;
    unsigned int next_read_pos;
    int sockclosed;
} lksocketreader_s;

lksocketreader_s *lksocketreader_new(int sock, size_t initial_size);
void lksocketreader_free(lksocketreader_s *sr);
ssize_t lksocketreader_readline(lksocketreader_s *sr, char *dst, size_t dst_len);
void lksocketreader_debugprint(lksocketreader_s *sr);


/*** lkhttprequestparser_s ***/
typedef struct {
    unsigned int nlinesread;             // number of lines read so far
    unsigned int header_content_length;  // value of previous Content-Length header parsed
    int head_complete;                  // flag indicating header lines complete
    int body_complete;                  // flag indicating request body (if needed) complete
    lkhttprequest_s *req;               // httprequest to output to while parsing
} lkhttprequestparser_s;

lkhttprequestparser_s *lkhttprequestparser_new();
void lkhttprequestparser_free(lkhttprequestparser_s *parser);
void lkhttprequestparser_reset(lkhttprequestparser_s *parser);
void lkhttprequestparser_parse_line(lkhttprequestparser_s *parser, char *line);


/*** socket helper functions ***/
int sock_recv(int sock, char *buf, size_t count, size_t *ret_nread);
int sock_send(int sock, char *buf, size_t count, size_t *ret_nsent);
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

