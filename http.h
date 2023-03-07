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

    // List of header key-value fields, terminated by NULL.
    // Ex.
    // [0] -> {"User-Agent", "browser"}
    // [1] -> {"From", "abc@email.com"}
    // NULL
    keyval_t *headers;
    size_t headers_size;
    size_t headers_len;

    int nparsedlines;
} httpreq_t;

httpreq_t *httpreq_new();
void httpreq_free(httpreq_t *req);
int httpreq_parseline(httpreq_t *req, char *line);

void httpreq_debugprint(httpreq_t *req);

#endif

