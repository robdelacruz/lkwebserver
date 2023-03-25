#ifndef NETFUNCS_H
#define NETFUNCS_H

#include "lklib.h"

/*** LKHttpRequest - HTTP Request struct ***/
typedef struct {
    LKString *method;       // GET
    LKString *uri;          // /path/to/index.html
    LKString *version;      // HTTP/1.0
    LKStringMap *headers;
    LKBuffer *head;
    LKBuffer *body;
} LKHttpRequest;

LKHttpRequest *lk_httprequest_new();
void lk_httprequest_free(LKHttpRequest *req);
void lk_httprequest_add_header(LKHttpRequest *req, char *k, char *v);
void lk_httprequest_append_body(LKHttpRequest *req, char *bytes, int bytes_len);
void lk_httprequest_debugprint(LKHttpRequest *req);


/*** LKHttpResponse - HTTP Response struct ***/
typedef struct {
    int status;             // 404
    LKString *statustext;    // File not found
    LKString *version;       // HTTP/1.0
    LKStringMap *headers;
    LKBuffer *head;
    LKBuffer *body;
} LKHttpResponse;

LKHttpResponse *lk_httpresponse_new();
void lk_httpresponse_free(LKHttpResponse *resp);
void lk_httpresponse_add_header(LKHttpResponse *resp, char *k, char *v);
void lk_httpresponse_gen_headbuf(LKHttpResponse *resp);
void lk_httpresponse_debugprint(LKHttpResponse *resp);


/*** LKSocketReader - Buffered input for sockets ***/
typedef struct {
    int sock;
    char *buf;
    size_t buf_size;
    size_t buf_len;
    unsigned int next_read_pos;
    int sockclosed;
} LKSocketReader;

LKSocketReader *lk_socketreader_new(int sock, size_t initial_size);
void lk_socketreader_free(LKSocketReader *sr);
ssize_t lk_socketreader_readline(LKSocketReader *sr, char *dst, size_t dst_len);
void lk_socketreader_debugprint(LKSocketReader *sr);


/*** LKHttpRequestParser ***/
typedef struct {
    unsigned int nlinesread;             // number of lines read so far
    unsigned int header_content_length;  // value of previous Content-Length header parsed
    int head_complete;                  // flag indicating header lines complete
    int body_complete;                  // flag indicating request body (if needed) complete
    LKHttpRequest *req;               // httprequest to output to while parsing
} LKHttpRequestParser;

LKHttpRequestParser *lk_httprequestparser_new();
void lk_httprequestparser_free(LKHttpRequestParser *parser);
void lk_httprequestparser_reset(LKHttpRequestParser *parser);
void lk_httprequestparser_parse_line(LKHttpRequestParser *parser, char *line);


/*** LKHttpServer ***/
typedef struct httpclientcontext LKHttpClientContext;
typedef void (*LKHttpHandlerFunc)(LKHttpRequest *req, LKHttpResponse *resp);

typedef struct {
    LKHttpClientContext *ctxhead;
    fd_set readfds;
    fd_set writefds;
    LKHttpHandlerFunc http_handler_func;
} LKHttpServer;

LKHttpServer *lk_httpserver_new(LKHttpHandlerFunc handlerfunc);
void lk_httpserver_free(LKHttpServer *server);
int lk_httpserver_serve(LKHttpServer *server, int listen_sock);


/*** socket helper functions ***/
int sock_recv(int sock, char *buf, size_t count, size_t *ret_nread);
int sock_send(int sock, char *buf, size_t count, size_t *ret_nsent);
void set_sock_timeout(int sock, int nsecs, int ms);
void set_sock_nonblocking(int sock);

/*** Other helper functions ***/
// Remove trailing CRLF or LF (\n) from string.
void chomp(char* s);
// Read entire file into buf.
ssize_t readfile(char *filepath, LKBuffer *buf);
// Read entire file descriptor contents into buf.
ssize_t readfd(int fd, LKBuffer *buf);
// Append src to dest, allocating new memory in dest if needed.
// Return new pointer to dest.
char *astrncat(char *dest, char *src, size_t src_len);

#endif

