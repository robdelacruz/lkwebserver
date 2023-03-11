#ifndef NETFUNCS_H
#define NETFUNCS_H

// sockbuf_t - Buffered input for sockets.
typedef struct {
    int sock;
    char *buf;
    size_t buf_size;
    size_t buf_len;
    unsigned int next_read_pos;
    int sockclosed;
} sockbuf_t;

// keyval_t - Key/Value pair
typedef struct {
    char *k;
    char *v;
} keyval_t;

// stringmap_t - Dynamic array of key-value fields
typedef struct {
    keyval_t *items;
    size_t items_len;
    size_t items_size;
} stringmap_t;

// buf_t - Dynamic bytes buffer
typedef struct {
    char *bytes;
    size_t bytes_len;
    size_t bytes_size;
} buf_t;

// httpreq_t - HTTP Request struct
typedef struct {
    char *method;       // GET
    char *uri;          // /path/to/index.html
    char *version;      // HTTP/1.0
    stringmap_t *headers;
    buf_t *body;

} httpreq_t;

// httpresp_t - HTTP Response struct
typedef struct {
    int status;         // 404
    char *statustext;   // File not found
    char *version;      // HTTP/1.0
    stringmap_t *headers;
    buf_t *body;
} httpresp_t;

stringmap_t *stringmap_new();
void stringmap_free(stringmap_t *sm);
void stringmap_set(stringmap_t *sm, char *k, char *v);

buf_t *buf_new(size_t bytes_size);
void buf_free(buf_t *buf);
int buf_append(buf_t *buf, char *bytes, size_t len);

/** Helper socket functions **/
ssize_t sock_recvn(int sock, char *buf, size_t count);
void set_sock_timeout(int sock, int nsecs, int ms);
void set_sock_nonblocking(int sock);

/** Other helper functions **/

// Remove trailing CRLF or LF (\n) from string.
void chomp(char* s);


/** sockbuf functions **/
sockbuf_t *sockbuf_new(int sock, size_t initial_size);
void free_sockbuf(sockbuf_t *sb);

// Read CR terminated line from socket using buffered input.
ssize_t sockbuf_readline(sockbuf_t *sb, char *dst, size_t dst_len);


/** httpreq functions **/
httpreq_t *httpreq_new();
void httpreq_free(httpreq_t *req);

// Parse http request initial line into req.
int httpreq_parse_request_line(httpreq_t *req, char *line);

// Parse an http header line into req.
int httpreq_parse_header_line(httpreq_t *req, char *line);

// Add a key/val http header into req.
void httpreq_add_header(httpreq_t *req, char *k, char *v);

// Append to req message body.
int httpreq_append_body(httpreq_t *req, char *bytes, int bytes_len);

// Print contents of req.
void httpreq_debugprint(httpreq_t *req);


/** httpresp functions **/
httpresp_t *httpresp_new();
void httpresp_free(httpresp_t *resp);
#endif

