#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lklib.h"
#include "lknet.h"

int lk_open_listen_socket(char *host, char *port, int backlog, struct sockaddr *psa) {
    int z;

    struct addrinfo hints, *ai;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    z = getaddrinfo(host, port, &hints, &ai);
    if (z != 0) {
        printf("getaddrinfo(): %s\n", gai_strerror(z));
        errno = EINVAL;
        return -1;
    }
    if (psa != NULL) {
        memcpy(psa, ai, sizeof(struct sockaddr));
    }

    int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd == -1) {
        lk_print_err("socket()");
        z = -1;
        goto error_return;
    }
    int yes=1;
    z = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (z == -1) {
        lk_print_err("setsockopt()");
        goto error_return;
    }
    z = bind(fd, ai->ai_addr, ai->ai_addrlen);
    if (z == -1) {
        lk_print_err("bind()");
        goto error_return;
    }
    z = listen(fd, backlog);
    if (z == -1) {
        lk_print_err("listen()");
        goto error_return;
    }

    freeaddrinfo(ai);
    return fd;

error_return:
    freeaddrinfo(ai);
    return z;
}

// You can specify the host and port in two ways:
// 1. host="littlekitten.xyz", port="5001" (separate host and port)
// 2. host="littlekitten.xyz:5001", port="" (combine host:port in host parameter)
int lk_open_connect_socket(char *host, char *port, struct sockaddr *psa) {
    int z;

    // If host is of the form "host:port", parse it.
    LKString *lksport = lk_string_new(host);
    LKStringList *ss = lk_string_split(lksport, ":");
    if (ss->items_len == 2) {
        host = ss->items[0]->s;
        port = ss->items[1]->s;
    }

    struct addrinfo hints, *ai;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    z = getaddrinfo(host, port, &hints, &ai);
    if (z != 0) {
        printf("getaddrinfo(): %s\n", gai_strerror(z));
        errno = EINVAL;
        return -1;
    }
    if (psa != NULL) {
        memcpy(psa, ai, sizeof(struct sockaddr));
    }

    lk_stringlist_free(ss);
    lk_string_free(lksport);

    int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd == -1) {
        lk_print_err("socket()");
        z = -1;
        goto error_return;
    }
    int yes=1;
    z = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (z == -1) {
        lk_print_err("setsockopt()");
        goto error_return;
    }
    z = connect(fd, ai->ai_addr, ai->ai_addrlen);
    if (z == -1) {
        lk_print_err("connect()");
        goto error_return;
    }

    freeaddrinfo(ai);
    return fd;

error_return:
    freeaddrinfo(ai);
    return z;
}


// Remove trailing CRLF or LF (\n) from string.
void lk_chomp(char* s) {
    int slen = strlen(s);
    for (int i=slen-1; i >= 0; i--) {
        if (s[i] != '\n' && s[i] != '\r') {
            break;
        }
        // Replace \n and \r with null chars. 
        s[i] = '\0';
    }
}

void lk_set_sock_timeout(int sock, int nsecs, int ms) {
    struct timeval tv;
    tv.tv_sec = nsecs;
    tv.tv_usec = ms * 1000; // convert milliseconds to microseconds
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
}

void lk_set_sock_nonblocking(int sock) {
    fcntl(sock, F_SETFL, O_NONBLOCK);
}

// Return sin_addr or sin6_addr depending on address family.
void *sockaddr_sin_addr(struct sockaddr *sa) {
    // addr->ai_addr is either struct sockaddr_in* or sockaddr_in6* depending on ai_family
    if (sa->sa_family == AF_INET) {
        struct sockaddr_in *p = (struct sockaddr_in*) sa;
        return &(p->sin_addr);
    } else {
        struct sockaddr_in6 *p = (struct sockaddr_in6*) sa;
        return &(p->sin6_addr);
    }
}
// Return sin_port or sin6_port depending on address family.
unsigned short lk_get_sockaddr_port(struct sockaddr *sa) {
    // addr->ai_addr is either struct sockaddr_in* or sockaddr_in6* depending on ai_family
    if (sa->sa_family == AF_INET) {
        struct sockaddr_in *p = (struct sockaddr_in*) sa;
        return ntohs(p->sin_port);
    } else {
        struct sockaddr_in6 *p = (struct sockaddr_in6*) sa;
        return ntohs(p->sin6_port);
    }
}

// Return human readable IP address from sockaddr
LKString *lk_get_ipaddr_string(struct sockaddr *sa) {
    char servipstr[INET6_ADDRSTRLEN];
    const char *pz = inet_ntop(sa->sa_family, sockaddr_sin_addr(sa),
                               servipstr, sizeof(servipstr));
    if (pz == NULL) {
        return lk_string_new("");
    }
    return lk_string_new(servipstr);
}

// Receive count bytes into buf nonblocking.
// Returns 0 for success, -1 for error.
// On return, ret_nread contains the number of bytes received.
int lk_sock_recv(int sock, char *buf, size_t count, size_t *ret_nread) {
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
int lk_sock_send(int sock, char *buf, size_t count, size_t *ret_nsent) {
    int nsent = 0;
    while (nsent < count) {
        int z = send(sock, buf+nsent, count-nsent, MSG_DONTWAIT | MSG_NOSIGNAL);
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

// Read count bytes into buf (supports nonblocking via open O_NONBLOCK flag).
// Returns 0 for success, -1 for error.
// On return, ret_nread contains the number of bytes read.
int lk_read(int fd, char *buf, size_t count, size_t *ret_nread) {
    size_t nread = 0;
    while (nread < count) {
        int z = read(fd, buf+nread, count-nread);
        // EOF
        if (z == 0) {
            break;
        }
        // interrupt occured during read, retry read.
        if (z == -1 && errno == EINTR) {
            continue;
        }
        if (z == -1) {
            // errno is set to EAGAIN/EWOULDBLOCK if fd is blocked
            *ret_nread = nread;
            return -1;
        }
        nread += z;
    }
    *ret_nread = nread;
    return 0;
}


/** lksocketreader functions **/

LKSocketReader *lk_socketreader_new(int sock, size_t buf_size) {
    LKSocketReader *sr = malloc(sizeof(LKSocketReader));

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

void lk_socketreader_free(LKSocketReader *sr) {
    free(sr->buf);
    sr->buf = NULL;

    free(sr);
}

// Read one line from buffered socket, including the \n char, \0 terminated.
// Return 0 for success, -1 for error.
// On return, ret_nreat contains the number of bytes received.
int lk_socketreader_readline(LKSocketReader *sr, char *dst, size_t dst_len, size_t *ret_nread) {
    assert(dst_len > 2); // Reserve space for \n and \0.
    assert(sr->buf_size >= sr->buf_len);
    assert(sr->buf_len >= sr->next_read_pos);

    if (sr->sockclosed) {
        *ret_nread = 0;
        return 0;
    }
    if (dst_len <= 2) {
        *ret_nread = 0;
        errno = EINVAL;
        return -1;
    }

    size_t nread = 0;
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
            // any other error
            if (z == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                assert(nread <= dst_len-1);
                dst[nread] = '\0';
                *ret_nread = nread;
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

    *ret_nread = nread;
    return 0;
}

int lk_socketreader_readbytes(LKSocketReader *sr, char *dst, size_t count, size_t *ret_nread) {
    size_t nread = 0;

    // Copy any unread buffer bytes into dst.
    if (sr->next_read_pos < sr->buf_len) {
        int ncopy = sr->buf_len - sr->next_read_pos;
        if (ncopy > count) {
            ncopy = count;
        }
        memcpy(dst, sr->buf + sr->next_read_pos, ncopy);
        sr->next_read_pos += ncopy;
        nread += ncopy;
    }

    // Read remaining bytes from socket until count reached.
    assert(count >= nread);
    if (count == nread) {
        *ret_nread = nread;
        return 0;
    }

    if (sr->sockclosed) {
        *ret_nread = nread;
        return 0;
    }
    size_t sock_nread = 0;
    int z = lk_socketreader_recv(sr, dst, count-nread, &sock_nread);
    if (z == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
        *ret_nread = nread;
        return z;
    }
    nread += sock_nread;
    *ret_nread = nread;
    return 0;
}

// Receive count bytes into buf nonblocking.
// Returns 0 for success, -1 for error.
// On return, ret_nread contains the number of bytes received.
int lk_socketreader_recv(LKSocketReader *sr, char *buf, size_t count, size_t *ret_nread) {
    size_t nread = 0;
    while (nread < count) {
        int z = recv(sr->sock, buf+nread, count-nread, MSG_DONTWAIT);
        // socket closed, no more data
        if (z == 0) {
            sr->sockclosed = 1;
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


void debugprint_buf(char *buf, size_t buf_size) {
    printf("buf: ");
    for (int i=0; i < buf_size; i++) {
        putchar(buf[i]);
    }
    putchar('\n');
    printf("buf_size: %ld\n", buf_size);
}

void lk_socketreader_debugprint(LKSocketReader *sr) {
    printf("buf_size: %ld\n", sr->buf_size);
    printf("buf_len: %ld\n", sr->buf_len);
    printf("next_read_pos: %d\n", sr->next_read_pos);
    printf("sockclosed: %d\n", sr->sockclosed);
    debugprint_buf(sr->buf, sr->buf_size);
    printf("\n");
}


/*** LKHttpRequest functions ***/
LKHttpRequest *lk_httprequest_new() {
    LKHttpRequest *req = malloc(sizeof(LKHttpRequest));
    req->method = lk_string_new("");
    req->uri = lk_string_new("");
    req->path = lk_string_new("");
    req->filename = lk_string_new("");
    req->querystring = lk_string_new("");
    req->version = lk_string_new("");
    req->headers = lk_stringtable_new();
    req->head = lk_buffer_new(0);
    req->body = lk_buffer_new(0);
    return req;
}

void lk_httprequest_free(LKHttpRequest *req) {
    lk_string_free(req->method);
    lk_string_free(req->uri);
    lk_string_free(req->path);
    lk_string_free(req->filename);
    lk_string_free(req->querystring);
    lk_string_free(req->version);
    lk_stringtable_free(req->headers);
    lk_buffer_free(req->head);
    lk_buffer_free(req->body);

    req->method = NULL;
    req->uri = NULL;
    req->path = NULL;
    req->filename = NULL;
    req->querystring = NULL;
    req->version = NULL;
    req->headers = NULL;
    req->head = NULL;
    req->body = NULL;
    free(req);
}

void lk_httprequest_add_header(LKHttpRequest *req, char *k, char *v) {
    lk_stringtable_set(req->headers, k, v);
}

void lk_httprequest_append_body(LKHttpRequest *req, char *bytes, int bytes_len) {
    lk_buffer_append(req->body, bytes, bytes_len);
}

void lk_httprequest_finalize(LKHttpRequest *req) {
    lk_buffer_clear(req->head);

    // Default to HTTP version.
    if (lk_string_sz_equal(req->version, "")) {
        lk_string_assign(req->version, "HTTP/1.0");
    }
    lk_buffer_append_sprintf(req->head, "%s %s %s\n", req->method->s, req->uri->s, req->version->s);
    if (req->body->bytes_len > 0) {
        lk_buffer_append_sprintf(req->head, "Content-Length: %ld\n", req->body->bytes_len);
    }
    for (int i=0; i < req->headers->items_len; i++) {
        lk_buffer_append_sprintf(req->head, "%s: %s\n", req->headers->items[i].k->s, req->headers->items[i].v->s);
    }
    lk_buffer_append(req->head, "\r\n", 2);
}


void lk_httprequest_debugprint(LKHttpRequest *req) {
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
        LKString *v = req->headers->items[i].v;
        printf("%s: %s\n", req->headers->items[i].k->s, v->s);
    }

    printf("Body:\n---\n");
    for (int i=0; i < req->body->bytes_len; i++) {
        putchar(req->body->bytes[i]);
    }
    printf("\n---\n");
}


/** httpresp functions **/
LKHttpResponse *lk_httpresponse_new() {
    LKHttpResponse *resp = malloc(sizeof(LKHttpResponse));
    resp->status = 0;
    resp->statustext = lk_string_new("");
    resp->version = lk_string_new("");
    resp->headers = lk_stringtable_new();
    resp->head = lk_buffer_new(0);
    resp->body = lk_buffer_new(0);
    return resp;
}

void lk_httpresponse_free(LKHttpResponse *resp) {
    lk_string_free(resp->statustext);
    lk_string_free(resp->version);
    lk_stringtable_free(resp->headers);
    lk_buffer_free(resp->head);
    lk_buffer_free(resp->body);

    resp->statustext = NULL;
    resp->version = NULL;
    resp->headers = NULL;
    resp->head = NULL;
    resp->body = NULL;
    free(resp);
}

void lk_httpresponse_add_header(LKHttpResponse *resp, char *k, char *v) {
    lk_stringtable_set(resp->headers, k, v);
}

// Finalize the http response by setting head buffer.
// Writes the status line, headers and CRLF blank string to head buffer.
void lk_httpresponse_finalize(LKHttpResponse *resp) {
    lk_buffer_clear(resp->head);

    // Default to 200 OK if no status set.
    if (resp->status == 0) {
        resp->status = 200;
        lk_string_assign(resp->statustext, "OK");
    }
    // Default to HTTP version.
    if (lk_string_sz_equal(resp->version, "")) {
        lk_string_assign(resp->version, "HTTP/1.0");
    }
    lk_buffer_append_sprintf(resp->head, "%s %d %s\n", resp->version->s, resp->status, resp->statustext->s);
    lk_buffer_append_sprintf(resp->head, "Content-Length: %ld\n", resp->body->bytes_len);
    for (int i=0; i < resp->headers->items_len; i++) {
        lk_buffer_append_sprintf(resp->head, "%s: %s\n", resp->headers->items[i].k->s, resp->headers->items[i].v->s);
    }
    lk_buffer_append(resp->head, "\r\n", 2);
}

void lk_httpresponse_debugprint(LKHttpResponse *resp) {
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
        LKString *v = resp->headers->items[i].v;
        printf("%s: %s\n", resp->headers->items[i].k->s, v->s);
    }

    printf("Body:\n---\n");
    for (int i=0; i < resp->body->bytes_len; i++) {
        putchar(resp->body->bytes[i]);
    }
    printf("\n---\n");
}

// Open and read entire file contents into buf.
// Return number of bytes read or -1 for error.
ssize_t lk_readfile(char *filepath, LKBuffer *buf) {
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        return -1;
    }
    int z = lk_readfd(fd, buf);
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
// Return number of bytes read or -1 for error.
#define TMPBUF_SIZE 512
ssize_t lk_readfd(int fd, LKBuffer *buf) {
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
        lk_buffer_append(buf, tmpbuf, z);
        nread += z;
    }
    return nread;
}

// Append src to dest, allocating new memory in dest if needed.
// Return new pointer to dest.
char *lk_astrncat(char *dest, char *src, size_t src_len) {
    int dest_len = strlen(dest);
    dest = realloc(dest, dest_len + src_len + 1);
    strncat(dest, src, src_len);
    return dest;
}

// Return whether file exists.
int lk_file_exists(char *filename) {
    struct stat statbuf;
    int z = stat(filename, &statbuf);
    return !z;
}

