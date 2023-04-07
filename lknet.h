#ifndef NETFUNCS_H
#define NETFUNCS_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
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
void lk_httpresponse_finalize(LKHttpResponse *resp);
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
int lk_socketreader_readline(LKSocketReader *sr, char *dst, size_t dst_len, size_t *ret_nread);
int lk_socketreader_readbytes(LKSocketReader *sr, char *dst, size_t count, size_t *ret_nread);
void lk_socketreader_debugprint(LKSocketReader *sr);


/*** LKHttpRequestParser ***/
typedef struct {
    LKString *partial_line;
    unsigned int nlinesread;
    int head_complete;              // flag indicating header lines complete
    int body_complete;              // flag indicating request body complete
    unsigned int content_length;    // value of Content-Length header
    LKHttpRequest *req;             // httprequest to output to while parsing
} LKHttpRequestParser;

LKHttpRequestParser *lk_httprequestparser_new();
void lk_httprequestparser_free(LKHttpRequestParser *parser);
void lk_httprequestparser_reset(LKHttpRequestParser *parser);
void lk_httprequestparser_parse_line(LKHttpRequestParser *parser, char *line);
void lk_httprequestparser_parse_bytes(LKHttpRequestParser *parser, char *buf, size_t buf_len);


/*** LKHttpServer ***/
typedef struct {
    LKString    *home_dir;
    LKString    *cgi_dir;
    LKStringMap *aliases;
} LKHttpServerSettings;

typedef struct httpclientcontext LKHttpClientContext;

typedef struct {
    LKHttpClientContext *ctxhead;
    fd_set readfds;
    fd_set writefds;
    LKHttpServerSettings *settings;
    void *handler_ctx;
} LKHttpServer;

LKHttpServerSettings *lk_httpserver_settings_new();
void lk_httpserver_settings_free(LKHttpServerSettings *settings);

LKHttpServer *lk_httpserver_new(LKHttpServerSettings *settings);
void lk_httpserver_free(LKHttpServer *server);
int lk_httpserver_serve(LKHttpServer *server, int listen_sock);


/*** socket helper functions ***/
int lk_sock_recv(int sock, char *buf, size_t count, size_t *ret_nread);
int lk_sock_send(int sock, char *buf, size_t count, size_t *ret_nsent);
void lk_set_sock_timeout(int sock, int nsecs, int ms);
void lk_set_sock_nonblocking(int sock);
LKString *lk_get_ipaddr_string(struct sockaddr *sa);

/*** Other helper functions ***/
// Remove trailing CRLF or LF (\n) from string.
void lk_chomp(char* s);
// Read entire file into buf.
ssize_t lk_readfile(char *filepath, LKBuffer *buf);
// Read entire file descriptor contents into buf.
ssize_t lk_readfd(int fd, LKBuffer *buf);
// Append src to dest, allocating new memory in dest if needed.
// Return new pointer to dest.
char *lk_astrncat(char *dest, char *src, size_t src_len);

#endif

