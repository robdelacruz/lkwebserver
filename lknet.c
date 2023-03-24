#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include "lklib.h"
#include "lknet.h"

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

// Receive count bytes into buf nonblocking.
// Returns 0 for success, -1 for error.
// On return, ret_nread contains the number of bytes received.
int sock_recv(int sock, char *buf, size_t count, size_t *ret_nread) {
    size_t nread = 0;
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
        if (z == -1) {
            // errno is set to EAGAIN/EWOULDBLOCK if socket is blocked
            // errno is set to EPIPE if socket was shutdown
            *ret_nread = nread;
            return -1;
        }
        nread += z;
    }
    *ret_nread = nread;
    return 0;
}

// Send count buf bytes into sock nonblocking.
// Returns 0 for success, -1 for error.
// On return, ret_nsent contains the number of bytes sent.
int sock_send(int sock, char *buf, size_t count, size_t *ret_nsent) {
    int nsent = 0;
    while (nsent < count) {
        int z = send(sock, buf+nsent, count-nsent, MSG_DONTWAIT);
        if (z == 0) {
            break;
        }
        // interrupt occured during send, retry send.
        if (z == -1 && errno == EINTR) {
            continue;
        }
        if (z == -1) {
            // errno is set to EAGAIN/EWOULDBLOCK if socket is blocked
            // errno is set to EPIPE if socket was shutdown
            *ret_nsent = nsent;
            return -1;
        }
        nsent += z;
    }
    *ret_nsent = nsent;
    return 0;
}

/** lksocketreader functions **/

lksocketreader_s *lksocketreader_new(int sock, size_t buf_size) {
    lksocketreader_s *sr = malloc(sizeof(lksocketreader_s));

    if (buf_size == 0) {
        buf_size = 1024;
    }
    sr->sock = sock;
    sr->buf_size = buf_size;
    sr->buf = malloc(buf_size);
    sr->buf_len = 0;
    sr->next_read_pos = 0;
    sr->sockclosed = 0;
    memset(sr->buf, '*', sr->buf_size); // initialize for debugging purposes.

    return sr;
}

void lksocketreader_free(lksocketreader_s *sr) {
    free(sr->buf);
    sr->buf = NULL;

    free(sr);
}

int lksocketreader_eof(lksocketreader_s *sr) {
    return sr->sockclosed;
}

// Read one line from buffered socket, including the \n char, \0 terminated.
// Returns num bytes read or -1 for error.
ssize_t lksocketreader_readline(lksocketreader_s *sr, char *dst, size_t dst_len) {
    assert(dst_len > 2); // Reserve space for \n and \0.
    assert(sr->buf_size >= sr->buf_len);
    assert(sr->buf_len >= sr->next_read_pos);

    if (sr->sockclosed) {
        return 0;
    }
    if (dst_len <= 2) {
        errno = EINVAL;
        return -1;
    }

    int nread = 0;
    while (nread < dst_len-1) { // leave space for null terminator
        // If no buffer chars available, read from socket.
        if (sr->next_read_pos >= sr->buf_len) {
            memset(sr->buf, '*', sr->buf_size); // initialize for debugging purposes.
            int z = recv(sr->sock, sr->buf, sr->buf_size, MSG_DONTWAIT);
            // socket closed, no more data
            if (z == 0) {
                sr->sockclosed = 1;
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
            sr->buf_len = z;
            sr->next_read_pos = 0;
        }

        // Copy unread buffer bytes into dst until a '\n' char.
        while (nread < dst_len-1) {
            if (sr->next_read_pos >= sr->buf_len) {
                break;
            }
            dst[nread] = sr->buf[sr->next_read_pos];
            nread++;
            sr->next_read_pos++;

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

void lksocketreader_debugprint(lksocketreader_s *sr) {
    printf("buf_size: %ld\n", sr->buf_size);
    printf("buf_len: %ld\n", sr->buf_len);
    printf("next_read_pos: %d\n", sr->next_read_pos);
    printf("sockclosed: %d\n", sr->sockclosed);
    debugprint_buf(sr->buf, sr->buf_size);
    printf("\n");
}


/*** httpreq functions ***/
lkhttprequest_s *lkhttprequest_new() {
    lkhttprequest_s *req = malloc(sizeof(lkhttprequest_s));
    req->method = lkstr_new("");
    req->uri = lkstr_new("");
    req->version = lkstr_new("");
    req->headers = lkstringmap_funcs_new(lkstr_voidp_free);
    req->head = lkbuf_new(0);
    req->body = lkbuf_new(0);
    return req;
}

void lkhttprequest_free(lkhttprequest_s *req) {
    lkstr_free(req->method);
    lkstr_free(req->uri);
    lkstr_free(req->version);
    lkstringmap_free(req->headers);
    lkbuf_free(req->head);
    lkbuf_free(req->body);

    req->method = NULL;
    req->uri = NULL;
    req->version = NULL;
    req->headers = NULL;
    req->head = NULL;
    req->body = NULL;
    free(req);
}

void lkhttprequest_add_header(lkhttprequest_s *req, char *k, char *v) {
    lkstringmap_set(req->headers, k, lkstr_new(v));
}

void lkhttprequest_append_body(lkhttprequest_s *req, char *bytes, int bytes_len) {
    lkbuf_append(req->body, bytes, bytes_len);
}

void lkhttprequest_debugprint(lkhttprequest_s *req) {
    assert(req->method != NULL);
    assert(req->uri != NULL);
    assert(req->version != NULL);
    assert(req->head != NULL);
    assert(req->body != NULL);

    printf("method: %s\n", req->method->s);
    printf("uri: %s\n", req->uri->s);
    printf("version: %s\n", req->version->s);

    printf("headers_size: %ld\n", req->headers->items_size);
    printf("headers_len: %ld\n", req->headers->items_len);

    printf("Headers:\n");
    for (int i=0; i < req->headers->items_len; i++) {
        printf("%s: %s\n", req->headers->items[i].k->s, (char *)req->headers->items[i].v);
    }

    printf("Body:\n");
    for (int i=0; i < req->body->bytes_len; i++) {
        putchar(req->body->bytes[i]);
    }
    printf("\n");
}


/*** lkhttprequestparser functions ***/
lkhttprequestparser_s *lkhttprequestparser_new() {
    lkhttprequestparser_s *parser = malloc(sizeof(lkhttprequestparser_s));
    parser->nlinesread = 0;
    parser->header_content_length = 0;
    parser->head_complete = 0;
    parser->body_complete = 0;
    parser->req = lkhttprequest_new();
    return parser;
}

void lkhttprequestparser_free(lkhttprequestparser_s *parser) {
    lkhttprequest_free(parser->req);
    parser->req = NULL;
    free(parser);
}

// Clear any pending state.
void lkhttprequestparser_reset(lkhttprequestparser_s *parser) {
    parser->nlinesread = 0;
    parser->header_content_length = 0;
    parser->head_complete = 0;
    parser->body_complete = 0;

    lkhttprequest_free(parser->req);
    parser->req = lkhttprequest_new();
}

// Parse initial request line in the format:
// GET /path/to/index.html HTTP/1.0
void parse_request_line(char *line, lkhttprequest_s *req) {
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

    char *method = "";
    char *uri = "";
    char *version = "";

    if (ntoksread > 0) method = toks[0];
    if (ntoksread > 1) uri = toks[1];
    if (ntoksread > 2) version = toks[2];

    lkstr_assign(req->method, method);
    lkstr_assign(req->uri, uri);
    lkstr_assign(req->version, version);

    free(linetmp);
}

void parse_body(char *bodybytes, size_t bodybytes_len, lkhttprequest_s *req) {
}

// Parse header line in the format Ex. User-Agent: browser
void parse_header_line(lkhttprequestparser_s *parser, char *line, lkhttprequest_s *req) {
    char *saveptr;
    char *delim = ":";

    char *linetmp = strdup(line);
    chomp(linetmp);
    char *k = strtok_r(linetmp, delim, &saveptr);
    if (k == NULL) {
        free(linetmp);
        return;
    }
    char *v = strtok_r(NULL, delim, &saveptr);
    if (v == NULL) {
        v = "";
    }

    // skip over leading whitespace
    while (*v == ' ' || *v == '\t') {
        v++;
    }
    lkhttprequest_add_header(req, k, v);

    if (!strcasecmp(k, "Content-Length")) {
        int content_length = atoi(v);
        parser->header_content_length = content_length;
    }

    free(linetmp);
}

// Return whether line is empty, ignoring whitespace chars ' ', \r, \n
int is_empty_line(char *s) {
    int slen = strlen(s);
    for (int i=0; i < slen; i++) {
        // Not an empty line if non-whitespace char is present.
        if (s[i] != ' ' && s[i] != '\n' && s[i] != '\r') {
            return 0;
        }
    }
    return 1;
}

// Parse one line and cumulatively compile results into parser->req.
// You can check the state of the parser through the following fields:
// parser->head_complete   Request Line and Headers complete
// parser->body_complete   httprequest is complete
void lkhttprequestparser_parse_line(lkhttprequestparser_s *parser, char *line) {
    // First line: parse initial request line.
    if (parser->nlinesread == 0) {
        parse_request_line(line, parser->req);
        parser->nlinesread++;
        return;
    }
    if (parser->body_complete) {
        return;
    }
    // Discard body lines if no Content-Length specified.
    if (parser->head_complete && parser->header_content_length == 0) {
        return;
    }
    // Append body line to http request.
    if (parser->head_complete) {
        lkbuf_append(parser->req->body, line, strlen(line));

        if (parser->req->body->bytes_len >= parser->header_content_length) {
            parser->body_complete = 1;
        }
        parser->nlinesread++;
        return;
    }
    // Empty CRLF line ends the headers section
    if (is_empty_line(line)) {
        parser->head_complete = 1;
        parser->nlinesread++;

        //$$ Set body_complete if GET or HEAD request regardless of Content-Length?
        if (parser->header_content_length == 0) {
            parser->body_complete = 1;
        }
        return;
    }
    // Header line
    if (!parser->head_complete) {
        parse_header_line(parser, line, parser->req);
        parser->nlinesread++;
        return;
    }
}


/** httpresp functions **/

lkhttpresponse_s *lkhttpresponse_new() {
    lkhttpresponse_s *resp = malloc(sizeof(lkhttpresponse_s));
    resp->status = 0;
    resp->statustext = lkstr_new("");
    resp->version = lkstr_new("");
    resp->headers = lkstringmap_funcs_new(lkstr_voidp_free);
    resp->head = lkbuf_new(0);
    resp->body = lkbuf_new(0);
    return resp;
}

void lkhttpresponse_free(lkhttpresponse_s *resp) {
    lkstr_free(resp->statustext);
    lkstr_free(resp->version);
    lkstringmap_free(resp->headers);
    lkbuf_free(resp->head);
    lkbuf_free(resp->body);

    resp->statustext = NULL;
    resp->version = NULL;
    resp->headers = NULL;
    resp->head = NULL;
    resp->body = NULL;
    free(resp);
}

void lkhttpresponse_add_header(lkhttpresponse_s *resp, char *k, char *v) {
    lkstringmap_set(resp->headers, k, lkstr_new(v));
}

void lkhttpresponse_gen_headbuf(lkhttpresponse_s *resp) {
    lkbuf_append_sprintf(resp->head, "%s %d %s\n", resp->version->s, resp->status, resp->statustext->s);
    lkbuf_append_sprintf(resp->head, "Content-Length: %ld\n", resp->body->bytes_len);
    lkbuf_append(resp->head, "\r\n", 2);
}

void lkhttpresponse_debugprint(lkhttpresponse_s *resp) {
    assert(resp->statustext != NULL);
    assert(resp->version != NULL);
    assert(resp->headers != NULL);
    assert(resp->head);
    assert(resp->body);

    printf("status: %d\n", resp->status);
    printf("statustext: %s\n", resp->statustext->s);
    printf("version: %s\n", resp->version->s);
    printf("headers_size: %ld\n", resp->headers->items_size);
    printf("headers_len: %ld\n", resp->headers->items_len);

    printf("Headers:\n");
    for (int i=0; i < resp->headers->items_len; i++) {
        printf("%s: %s\n", resp->headers->items[i].k->s, (char *)resp->headers->items[i].v);
    }

    printf("Body:\n");
    for (int i=0; i < resp->body->bytes_len; i++) {
        putchar(resp->body->bytes[i]);
    }
    printf("\n");
}

// Open and read entire file contents into buf.
ssize_t readfile(char *filepath, lkbuf_s *buf) {
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        return -1;
    }
    int z = readfd(fd, buf);
    if (z == -1) {
        int tmperrno = errno;
        close(fd);
        errno = tmperrno;
        return z;
    }

    close(fd);
    return z;
}

// Read entire file descriptor contents into buf.
#define TMPBUF_SIZE 512
ssize_t readfd(int fd, lkbuf_s *buf) {
    char tmpbuf[TMPBUF_SIZE];

    int nread = 0;
    while (1) {
        int z = read(fd, tmpbuf, TMPBUF_SIZE);
        if (z == -1 && errno == EINTR) {
            continue;
        }
        if (z == -1) {
            return z;
        }
        if (z == 0) {
            break;
        }
        lkbuf_append(buf, tmpbuf, z);
        nread += z;
    }
    return nread;
}

// Append src to dest, allocating new memory in dest if needed.
// Return new pointer to dest.
char *astrncat(char *dest, char *src, size_t src_len) {
    int dest_len = strlen(dest);
    dest = realloc(dest, dest_len + src_len + 1);
    strncat(dest, src, src_len);
    return dest;
}

