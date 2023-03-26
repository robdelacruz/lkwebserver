#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <sys/wait.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "lknet.h"

#define LISTEN_PORT "5000"

void print_err(char *s);
void exit_err(char *s);
void *addrinfo_sin_addr(struct addrinfo *addr);
void handle_sigint(int sig);
void handle_sigchld(int sig);

void serve_file_handler(void *handler_ctx, LKHttpRequest *req, LKHttpResponse *resp);
int is_valid_http_method(char *method);
char *fileext(char *filepath);

typedef struct {
    char *rootdir;
} FileHandlerCtx;

int main(int argc, char *argv[]) {
    int z;

    signal(SIGINT, handle_sigint);
    signal(SIGCHLD, handle_sigchld);

    // Get this server's address.
    struct addrinfo hints, *servaddr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    z = getaddrinfo("localhost", LISTEN_PORT, &hints, &servaddr);
    if (z != 0) {
        exit_err("getaddrinfo()");
    }

    // Get human readable IP address string in servipstr.
    char servipstr[INET6_ADDRSTRLEN];
    const char *pz = inet_ntop(servaddr->ai_family, addrinfo_sin_addr(servaddr), servipstr, sizeof(servipstr));
    if (pz ==NULL) {
        exit_err("inet_ntop()");
    }

    int s0 = socket(servaddr->ai_family, servaddr->ai_socktype, servaddr->ai_protocol);
    if (s0 == -1) {
        exit_err("socket()");
    }

    int yes=1;
    z = setsockopt(s0, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (s0 == -1) {
        exit_err("setsockopt(SO_REUSEADDR)");
    }

    z = bind(s0, servaddr->ai_addr, servaddr->ai_addrlen);
    if (z != 0) {
        exit_err("bind()");
    }

    freeaddrinfo(servaddr);
    servaddr = NULL;

    FileHandlerCtx handler_ctx;
    handler_ctx.rootdir = "/home/rob";

    LKHttpServer *httpserver = lk_httpserver_new(serve_file_handler, (void*) &handler_ctx);

    printf("Listening on %s:%s...\n", servipstr, LISTEN_PORT);
    z = lk_httpserver_serve(httpserver, s0);
    if (z == -1) {
        exit_err("lk_httpserver_serve()");
    }

    // Code doesn't reach here.
    lk_httpserver_free(httpserver);
    close(s0);
    return 0;
}

// Print the last error message corresponding to errno.
void print_err(char *s) {
    fprintf(stderr, "%s: %s\n", s, strerror(errno));
}
void exit_err(char *s) {
    print_err(s);
    exit(1);
}

// Return sin_addr or sin6_addr depending on address family.
void *addrinfo_sin_addr(struct addrinfo *addr) {
    // addr->ai_addr is either struct sockaddr_in* or sockaddr_in6* depending on ai_family
    if (addr->ai_family == AF_INET) {
        struct sockaddr_in *p = (struct sockaddr_in*) addr->ai_addr;
        return &(p->sin_addr);
    } else {
        struct sockaddr_in6 *p = (struct sockaddr_in6*) addr->ai_addr;
        return &(p->sin6_addr);
    }
}

void handle_sigint(int sig) {
    printf("SIGINT received\n");
    fflush(stdout);
    exit(0);
}

void handle_sigchld(int sig) {
    int tmp_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
    errno = tmp_errno;
}

// Generate an http response to an http request.
void serve_file_handler(void *handler_ctx, LKHttpRequest *req, LKHttpResponse *resp) {
    FileHandlerCtx *ctx = handler_ctx;
    printf("serve_file_handler() ctx->rootdir: '%s'\n", ctx->rootdir);
    int z;

    static char *html_error_start = 
       "<!DOCTYPE html>\n"
       "<html>\n"
       "<head><title>Error response</title></head>\n"
       "<body><h1>Error response</h1>\n";
    static char *html_error_end =
       "</body></html>\n";

    static char *html_sample =
       "<!DOCTYPE html>\n"
       "<html>\n"
       "<head><title>Little Kitten</title></head>\n"
       "<body><h1>Little Kitten webserver</h1>\n"
       "<p>Hello Little Kitten!</p>\n"
       "</body></html>\n";

    char *method = req->method->s;
    char *uri = req->uri->s;

    //$$ instead of this hack, check index.html, index.html, default.html if exists
    if (!strcmp(uri, "/")) {
        uri = "/index.html";
    }

    if (!is_valid_http_method(method)) {
        resp->status = 501;
        lk_string_sprintf(resp->statustext, "Unsupported method ('%s')", method);
        lk_string_assign(resp->version, "HTTP/1.0");

        lk_buffer_append(resp->body, html_error_start, strlen(html_error_start));
        lk_buffer_append_sprintf(resp->body, "<p>%d %s</p>\n", resp->status, resp->statustext->s);
        lk_buffer_append(resp->body, html_error_end, strlen(html_error_end));
        lk_httpresponse_gen_headbuf(resp);
        return;
    }
    if (!strcmp(method, "GET")) {
        resp->status = 200;
        lk_string_assign(resp->statustext, "OK");
        lk_string_assign(resp->version, "HTTP/1.0");

        // /littlekitten sample page
        if (!strcmp(uri, "/littlekitten")) {
            lk_httpresponse_add_header(resp, "Content-Type", "text/html");
            lk_buffer_append(resp->body, html_sample, strlen(html_sample));
            lk_httpresponse_gen_headbuf(resp);
            return;
        }

        LKString *content_type = lk_string_new("");
        lk_string_sprintf(content_type, "text/%s;", fileext(uri));
        lk_httpresponse_add_header(resp, "Content-Type", content_type->s);
        lk_string_free(content_type);

        // uri_filepath = current dir + uri
        // Ex. "/path/to" + "/index.html"
        char *tmp_currentdir = get_current_dir_name();
        LKString *uri_filepath = lk_string_new(tmp_currentdir);
        lk_string_append(uri_filepath, uri);
        free(tmp_currentdir);

        printf("uri_filepath: %s\n", uri_filepath->s);
        z = lk_readfile(uri_filepath->s, resp->body);
        if (z == -1) {
            // uri file not found
            resp->status = 404;
            lk_string_sprintf(resp->statustext, "File not found '%s'", uri);
        }
        lk_string_free(uri_filepath);

        lk_httpresponse_gen_headbuf(resp);
        return;
    }

    // Return default response 200 OK.
    resp->status = 200;
    lk_string_assign(resp->statustext, "Default OK");
    lk_string_assign(resp->version, "HTTP/1.0");
    lk_httpresponse_gen_headbuf(resp);
}

int is_valid_http_method(char *method) {
    if (method == NULL) {
        return 0;
    }

    if (!strcasecmp(method, "GET")      ||
        !strcasecmp(method, "POST")     || 
        !strcasecmp(method, "PUT")      || 
        !strcasecmp(method, "DELETE")   ||
        !strcasecmp(method, "HEAD"))  {
        return 1;
    }

    return 0;
}

// Return ptr to start of file extension within filepath.
// Ex. "path/to/index.html" returns "index.html"
char *fileext(char *filepath) {
    int filepath_len = strlen(filepath);
    // filepath of "" returns ext of "".
    if (filepath_len == 0) {
        return filepath;
    }

    char *p = filepath + strlen(filepath) - 1;
    while (p >= filepath) {
        if (*p == '.') {
            return p+1;
        }
        p--;
    }
    return filepath;
}

