#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include "netfuncs.h"

// Remove trailing CRLF or LF (\n) from string.
void chomp(char* s) {
    int slen = strlen(s);
    for (int i=slen-1; i >= 0; i--) {
        if (s[i] != '\n' && s[i] != '\r') {
            break;
        }
        // Replace \n and \r with null chars. 
        s[i] = '\0';
    }
}

void set_sock_timeout(int sock, int nsecs, int ms) {
    struct timeval tv;
    tv.tv_sec = nsecs;
    tv.tv_usec = ms * 1000; // convert milliseconds to microseconds
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
}

void set_sock_nonblocking(int sock) {
    fcntl(sock, F_SETFL, O_NONBLOCK);
}

// Receive count bytes into buf.
// Returns num bytes received or -1 for error.
ssize_t sock_recvn(int sock, char *buf, size_t count) {
    memset(buf, '*', count); // initialize for debugging purposes.

    int nread = 0;
    while (nread < count) {
        int z = recv(sock, buf+nread, count-nread, MSG_DONTWAIT);
        // socket closed, no more data
        if (z == 0) {
            break;
        }
        // interrupt occured during read, retry read.
        if (z == -1 && errno == EINTR) {
            continue;
        }
        // no data available at the moment, just return what we have.
        if (z == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        // any other error
        if (z == -1) {
            return z;
        }
        nread += z;
    }

    return nread;
}

/** sockbuf functions **/

sockbuf_t *sockbuf_new(int sock, size_t buf_size) {
    sockbuf_t *sb = malloc(sizeof(sockbuf_t));

    if (buf_size == 0) {
        buf_size = 1024;
    }
    sb->sock = sock;
    sb->buf_size = buf_size;
    sb->buf = malloc(buf_size);
    sb->buf_len = 0;
    sb->next_read_pos = 0;
    sb->sockclosed = 0;
    memset(sb->buf, '*', sb->buf_size); // initialize for debugging purposes.

    return sb;
}

void sockbuf_free(sockbuf_t *sb) {
    free(sb->buf);
    sb->buf = NULL;

    free(sb);
}

int sockbuf_eof(sockbuf_t *sb) {
    return sb->sockclosed;
}

// Read one line from buffered socket, including the \n char, \0 terminated.
// Returns num bytes read or -1 for error.
ssize_t sockbuf_readline(sockbuf_t *sb, char *dst, size_t dst_len) {
    assert(dst_len > 2); // Reserve space for \n and \0.
    assert(sb->buf_size >= sb->buf_len);
    assert(sb->buf_len >= sb->next_read_pos);

    if (sb->sockclosed) {
        return 0;
    }
    if (dst_len <= 2) {
        errno = EINVAL;
        return -1;
    }

    int nread = 0;
    while (nread < dst_len-1) { // leave space for null terminator
        // If no buffer chars available, read from socket.
        if (sb->next_read_pos >= sb->buf_len) {
            memset(sb->buf, '*', sb->buf_size); // initialize for debugging purposes.
            int z = recv(sb->sock, sb->buf, sb->buf_size, MSG_DONTWAIT);
            // socket closed, no more data
            if (z == 0) {
                sb->sockclosed = 1;
                break;
            }
            // interrupt occured during read, retry read.
            if (z == -1 && errno == EINTR) {
                continue;
            }
            // no data available at the moment, just return what we have.
            if (z == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                break;
            }
            // any other error
            if (z == -1) {
                assert(nread <= dst_len-1);
                dst[nread] = '\0';
                return z;
            }
            sb->buf_len = z;
            sb->next_read_pos = 0;
        }

        // Copy unread buffer bytes into dst until a '\n' char.
        while (nread < dst_len-1) {
            if (sb->next_read_pos >= sb->buf_len) {
                break;
            }
            dst[nread] = sb->buf[sb->next_read_pos];
            nread++;
            sb->next_read_pos++;

            if (dst[nread-1] == '\n') {
                goto readline_end;
            }
        }
    }

readline_end:
    assert(nread <= dst_len-1);
    dst[nread] = '\0';
    return nread;
}

void debugprint_buf(char *buf, size_t buf_size) {
    printf("buf: ");
    for (int i=0; i < buf_size; i++) {
        putchar(buf[i]);
    }
    putchar('\n');
    printf("buf_size: %ld\n", buf_size);
}

void sockbuf_debugprint(sockbuf_t *sb) {
    printf("buf_size: %ld\n", sb->buf_size);
    printf("buf_len: %ld\n", sb->buf_len);
    printf("next_read_pos: %d\n", sb->next_read_pos);
    printf("sockclosed: %d\n", sb->sockclosed);
    debugprint_buf(sb->buf, sb->buf_size);
    printf("\n");
}

/** httpreq functions **/

keyval_t *create_headers(size_t num_headers) {
    keyval_t *headers = malloc(num_headers * sizeof(keyval_t));
    memset(headers, 0, num_headers * sizeof(keyval_t));
    return headers;
}

void free_headers(keyval_t *headers, size_t num_headers) {
    for (int i=0; i < num_headers; i++) {
        free(headers[i].k);
        free(headers[i].v);
    }
    free(headers);
}

httpreq_t *httpreq_new() {
    httpreq_t *req = malloc(sizeof(httpreq_t));
    req->method = NULL;
    req->uri = NULL;
    req->version = NULL;
    req->body = NULL;
    req->body_len = 0;

    req->headers_size = 10; // start with room for n headers
    req->headers_len = 0;
    req->headers = create_headers(req->headers_size);
}

void httpreq_free(httpreq_t *req) {
    if (req->method) free(req->method);
    if (req->uri) free(req->uri);
    if (req->version) free(req->version);
    if (req->body) free(req->body);
    free_headers(req->headers, req->headers_len);

    req->method = NULL;
    req->uri = NULL;
    req->version = NULL;
    req->body = NULL;
    req->headers = NULL;
    free(req);
}

// Parse initial request line
// Ex. GET /path/to/index.html HTTP/1.0
int httpreq_parse_request_line(httpreq_t *req, char *line) {
    char *toks[3];
    int ntoksread = 0;

    char *saveptr;
    char *delim = " \t";
    char *linetmp = strdup(line);
    chomp(linetmp);
    char *p = linetmp;
    while (ntoksread < 3) {
        toks[ntoksread] = strtok_r(p, delim, &saveptr);
        if (toks[ntoksread] == NULL) {
            break;
        }
        ntoksread++;
        p = NULL; // for the next call to strtok_r()
    }

    if (ntoksread < 3) {
        free(linetmp);
        return -1;
    }

    char *method = toks[0];
    char *uri = toks[1];
    char *version = toks[2];

    if (strcmp(method, "GET") != 0 &&
        strcmp(method, "POST") != 0 && 
        strcmp(method, "PUT") != 0 && 
        strcmp(method, "DELETE") != 0) {
        free(linetmp);
        return -1;
    }

    req->method = strdup(method);
    req->uri = strdup(uri);
    req->version = strdup(version);

    free(linetmp);
    return 0;
}

void httpreq_add_header(httpreq_t *req, char *k, char *v) {
    assert(req->headers_size >= req->headers_len);

    if (req->headers_len == req->headers_size) {
        req->headers_size += 10;
        req->headers = realloc(req->headers, req->headers_size * sizeof(keyval_t));
    }

    req->headers[req->headers_len].k = strdup(k);
    req->headers[req->headers_len].v = strdup(v);
    req->headers_len++;
}

// Parse header line
// Ex. User-Agent: browser
int httpreq_parse_header_line(httpreq_t *req, char *line) {
    char *saveptr;
    char *delim = ":";

    char *linetmp = strdup(line);
    chomp(linetmp);
    char *k = strtok_r(linetmp, delim, &saveptr);
    if (k == NULL) {
        free(linetmp);
        return -1;
    }
    char *v = strtok_r(NULL, delim, &saveptr);
    if (v == NULL) {
        free(linetmp);
        return -1;
    }

    // skip over leading whitespace
    while (*v == ' ' || *v == '\t') {
        v++;
    }
    httpreq_add_header(req, k, v);

    free(linetmp);
    return 0;
}

int httpreq_append_body(httpreq_t *req, char *bytes, int bytes_len) {
    // First time setting the body.
    if (req->body == NULL) {
        req->body = malloc(bytes_len);
        if (req->body == NULL) {
            return -1;
        }
        memcpy(req->body, bytes, bytes_len);
        req->body_len = bytes_len;
        return 0;
    }

    // Append bytes to existing body.
    req->body = realloc(req->body, req->body_len + bytes_len);
    if (req->body == NULL) {
        return -1;
    }
    memcpy(req->body + req->body_len, bytes, bytes_len);
    req->body_len += bytes_len;
    return 0;
}

void httpreq_debugprint(httpreq_t *req) {
    if (req->method) {
        printf("method: %s\n", req->method);
    }
    if (req->uri) {
        printf("uri: %s\n", req->uri);
    }
    if (req->version) {
        printf("version: %s\n", req->version);
    }
    printf("headers_size: %ld\n", req->headers_size);
    printf("headers_len: %ld\n", req->headers_len);

    printf("Headers:\n");
    for (int i=0; i < req->headers_len; i++) {
        printf("%s: %s\n", req->headers[i].k, req->headers[i].v);
    }

    if (req->body) {
        printf("Body:\n");
        for (int i=0; i < req->body_len; i++) {
            putchar(req->body[i]);
        }
        printf("\n");
    }
}

/** httpresp functions **/

httpresp_t *httpresp_new() {
    httpresp_t *resp = malloc(sizeof(httpresp_t));
    resp->status = 0;
    resp->statustext = NULL;
    resp->version = NULL;
    resp->body = NULL;
    resp->body_len = 0;

    resp->headers_size = 10; // start with room for n headers
    resp->headers_len = 0;
    resp->headers = create_headers(resp->headers_size);
}

void httpresp_free(httpresp_t *resp) {
    if (resp->statustext) free(resp->statustext);
    if (resp->version) free(resp->version);
    if (resp->body) free(resp->body);
    free_headers(resp->headers, resp->headers_len);

    resp->statustext = NULL;
    resp->version = NULL;
    resp->body = NULL;
    resp->headers = NULL;
    free(resp);
}

