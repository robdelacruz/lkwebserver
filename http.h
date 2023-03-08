#ifndef HTTP_H
#define HTTP_H

// Key-value pair
typedef struct {
    char *k;
    char *v;
} keyval_t;

// HTTP Request struct
typedef struct {
    char *method;       // GET
    char *uri;          // /path/to/index.html
    char *version;      // HTTP/1.0
    char *body;
    size_t body_len;

    // List of header key-value fields, terminated by NULL.
    // Ex.
    // [0] -> {"User-Agent", "browser"}
    // [1] -> {"From", "abc@email.com"}
    // NULL
    keyval_t *headers;
    size_t headers_size;
    size_t headers_len;
} httpreq_t;

httpreq_t *httpreq_new();
void httpreq_free(httpreq_t *req);
int httpreq_parse_request_line(httpreq_t *req, char *line);
int httpreq_parse_header_line(httpreq_t *req, char *line);
int httpreq_append_body(httpreq_t *req, char *bytes, int bytes_len);

void httpreq_add_header(httpreq_t *req, char *k, char *v);

void httpreq_debugprint(httpreq_t *req);

#endif

