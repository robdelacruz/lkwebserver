#ifndef LKNET_H
#define LKNET_H

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
    LKString *uri;          // "/path/to/index.html?p=1&start=5"
    LKString *path;         // "/path/to/index.html"
    LKString *filename;     // "index.html"
    LKString *querystring;  // "p=1&start=5"
    LKString *version;      // HTTP/1.0
    LKStringTable *headers;
    LKBuffer *head;
    LKBuffer *body;
} LKHttpRequest;

LKHttpRequest *lk_httprequest_new();
void lk_httprequest_free(LKHttpRequest *req);
void lk_httprequest_add_header(LKHttpRequest *req, char *k, char *v);
void lk_httprequest_append_body(LKHttpRequest *req, char *bytes, int bytes_len);
void lk_httprequest_finalize(LKHttpRequest *req);
void lk_httprequest_debugprint(LKHttpRequest *req);


/*** LKHttpResponse - HTTP Response struct ***/
typedef struct {
    int status;             // 404
    LKString *statustext;    // File not found
    LKString *version;       // HTTP/1.0
    LKStringTable *headers;
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
int lk_socketreader_recv(LKSocketReader *sr, char *buf, size_t count, size_t *ret_nread);
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

LKHttpRequestParser *lk_httprequestparser_new(LKHttpRequest *req);
void lk_httprequestparser_free(LKHttpRequestParser *parser);
void lk_httprequestparser_reset(LKHttpRequestParser *parser);
void lk_httprequestparser_parse_line(LKHttpRequestParser *parser, char *line);
void lk_httprequestparser_parse_bytes(LKHttpRequestParser *parser, char *buf, size_t buf_len);

/*** CGI Parser ***/
void parse_cgi_output(LKBuffer *buf, LKHttpResponse *resp);


/*** LKContext ***/
typedef enum {
    CTX_READ_REQ,
    CTX_READ_CGI_OUTPUT,
    CTX_WRITE_CGI_INPUT,
    CTX_WRITE_RESP,
    CTX_PROXY_WRITE_REQ,
    CTX_PROXY_READ_RESP,
    CTX_PROXY_WRITE_RESP
} LKContextType;

typedef struct lkcontext_s {
    int selectfd;
    int clientfd;
    LKContextType type;
    struct lkcontext_s *next;         // link to next ctx

    // Used by CTX_READ_REQ:
    struct sockaddr_in client_sa;     // client address
    LKString *client_ipaddr;          // client ip address string
    unsigned short client_port;       // client port number
    LKSocketReader *sr;               // input buffer for reading lines
    LKHttpRequestParser *reqparser;   // parser for httprequest
    LKHttpRequest *req;               // http request so far
    LKHttpResponse *resp;             // http response to be sent
                                      //
    // Used by CTX_READ_CGI:
    int cgifd;
    LKBuffer *cgi_outputbuf;          // receive cgi stdout bytes here
    LKBuffer *cgi_inputbuf;           // input bytes to pass to cgi stdin

    // Used by CTX_PROXY_WRITE_REQ:
    int proxyfd;
    LKBuffer *proxy_respbuf;
} LKContext;

LKContext *lk_context_new();
LKContext *create_initial_context(int fd, struct sockaddr_in *sa);
void lk_context_free(LKContext *ctx);

void add_new_client_context(LKContext **pphead, LKContext *ctx);
void add_context(LKContext **pphead, LKContext *ctx);
int remove_client_context(LKContext **pphead, int clientfd);
void remove_client_contexts(LKContext **pphead, int clientfd);
LKContext *match_select_ctx(LKContext *phead, int selectfd);
int remove_selectfd_context(LKContext **pphead, int selectfd);


/*** LKConfig ***/
typedef struct {
    LKString *hostname;
    LKString *homedir;
    LKString *homedir_abspath;
    LKString *cgidir;
    LKStringTable *aliases;
    LKString *proxyhost;
} LKHostConfig;

typedef struct {
    LKString *serverhost;
    LKString *port;
    LKHostConfig **hostconfigs;
    size_t hostconfigs_len;
    size_t hostconfigs_size;
} LKConfig;

LKConfig *lk_config_new();
void lk_config_free(LKConfig *cfg);
int lk_config_read_configfile(LKConfig *cfg, char *configfile);
void lk_config_print(LKConfig *cfg);
LKHostConfig *lk_config_add_hostconfig(LKConfig *cfg, LKHostConfig *hc);
LKHostConfig *lk_config_find_hostconfig(LKConfig *cfg, char *hostname);
LKHostConfig *lk_config_create_get_hostconfig(LKConfig *cfg, char *hostname);
void lk_config_finalize(LKConfig *cfg);

LKHostConfig *lk_hostconfig_new(char *hostname);
void lk_hostconfig_free(LKHostConfig *hc);


typedef struct {
    LKConfig *cfg;
    LKContext *ctxhead;
    fd_set readfds;
    fd_set writefds;
    int maxfd;
} LKHttpServer;

typedef enum {
    LKHTTPSERVEROPT_HOMEDIR,
    LKHTTPSERVEROPT_PORT,
    LKHTTPSERVEROPT_HOST,
    LKHTTPSERVEROPT_CGIDIR,
    LKHTTPSERVEROPT_ALIAS,
    LKHTTPSERVEROPT_PROXYPASS
} LKHttpServerOpt;

LKHttpServer *lk_httpserver_new(LKConfig *cfg);
void lk_httpserver_free(LKHttpServer *server);
void lk_httpserver_setopt(LKHttpServer *server, LKHttpServerOpt opt, ...);
int lk_httpserver_serve(LKHttpServer *server);


/*** Helper functions ***/
int lk_open_listen_socket(char *host, char *port, int backlog, struct sockaddr *psa);
int lk_open_connect_socket(char *host, char *port, struct sockaddr *psa);
void lk_set_sock_timeout(int sock, int nsecs, int ms);
void lk_set_sock_nonblocking(int sock);
LKString *lk_get_ipaddr_string(struct sockaddr *sa);
unsigned short lk_get_sockaddr_port(struct sockaddr *sa);
int nonblocking_error(int z);

typedef enum {FD_SOCK, FD_FILE} FDType;
typedef enum {FD_READ, FD_WRITE, FD_READWRITE} FDAction;

int lk_write_fd(int fd, FDType fd_type, char *buf, size_t count, size_t *nbytes);
int lk_send(int fd, char *buf, size_t count, size_t *nbytes);
int lk_write(int fd, char *buf, size_t count, size_t *nbytes);

int lk_read_fd(int fd, FDType fd_type, char *buf, size_t count, size_t *nbytes);
int lk_recv(int fd, char *buf, size_t count, size_t *nbytes);
int lk_read(int fd, char *buf, size_t count, size_t *nbytes);

int lk_write_buf_fd(int fd, FDType fd_type, LKBuffer *buf);
int lk_send_buf(int fd, LKBuffer *buf);
int lk_write_buf(int fd, LKBuffer *buf);

int lk_read_buf_fd(int fd, FDType fd_type, LKBuffer *buf);
int lk_recv_buf(int fd, LKBuffer *buf);
int lk_read_buf(int fd, LKBuffer *buf);

// Remove trailing CRLF or LF (\n) from string.
void lk_chomp(char* s);
// Read entire file into buf.
ssize_t lk_readfile(char *filepath, LKBuffer *buf);
// Read entire file descriptor contents into buf.
ssize_t lk_readfd(int fd, LKBuffer *buf);
// Append src to dest, allocating new memory in dest if needed.
// Return new pointer to dest.
char *lk_astrncat(char *dest, char *src, size_t src_len);
// Return whether file exists.
int lk_file_exists(char *filename);

#endif

