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
int read_uri_file(char *home_dir, char *uri, LKBuffer *buf);
int is_valid_http_method(char *method);
char *fileext(char *filepath);

typedef struct {
    LKString    *home_dir;
    LKString    *cgi_dir;
    LKStringMap *aliases;
} FileHandlerSettings;

FileHandlerSettings *create_filehandler_settings(char *sz_home_dir, char *sz_cgi_dir) {
    FileHandlerSettings *fhs = malloc(sizeof(FileHandlerSettings));
    fhs->home_dir = lk_string_new(sz_home_dir);
    fhs->cgi_dir = lk_string_new(sz_cgi_dir);
    fhs->aliases = lk_stringmap_new();
    return fhs;
}

void free_filehandler_settings(FileHandlerSettings *settings) {
    lk_string_free(settings->home_dir);
    lk_string_free(settings->cgi_dir);
    lk_stringmap_free(settings->aliases);
    free(settings);
}

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

    char *home_dir = "";
    char *cgi_dir = "";

    // webserver <home_dir> <cgi_dir>
    // argv[0]   argv[1]    argv[2]
    if (argc > 1) home_dir = argv[1];
    if (argc > 2) cgi_dir = argv[2];

    FileHandlerSettings *settings = create_filehandler_settings(home_dir, cgi_dir);
    lk_stringmap_set(settings->aliases, "latest", "/latest.html");
    lk_stringmap_set(settings->aliases, "about", "/about.html");

    LKHttpServer *httpserver = lk_httpserver_new(serve_file_handler, (void*) settings);

    printf("Listening on %s:%s...\n", servipstr, LISTEN_PORT);
    z = lk_httpserver_serve(httpserver, s0);
    if (z == -1) {
        exit_err("lk_httpserver_serve()");
    }

    // Code doesn't reach here.
    free_filehandler_settings(settings);
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
//    printf("serve_file_handler():\n");
//    lk_httprequest_debugprint(req);

    int z;
    FileHandlerSettings *settings = handler_ctx;

    // If home_dir not specified, use current working directory.
    if (settings->home_dir->s_len == 0) {
        char *current_dir = get_current_dir_name();
        if (current_dir == NULL) {
            resp->status = 500;
            lk_string_assign(resp->statustext, "Server error reading directory");
            return;
        }
        lk_string_append(settings->home_dir, current_dir);
        free(current_dir);
    }

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

    static char *html_start =
       "<!DOCTYPE html>\n"
       "<html>\n"
       "<head><title>Little Kitten Sample Response</title></head>\n"
       "<body>\n";
    static char *html_end =
       "</body></html>\n";

    char *method = req->method->s;
    char *uri = req->uri->s;

    if (!is_valid_http_method(method)) {
        resp->status = 501;
        lk_string_sprintf(resp->statustext, "Unsupported method ('%s')", method);

        lk_buffer_append(resp->body, html_error_start, strlen(html_error_start));
        lk_buffer_append_sprintf(resp->body, "<p>%d %s</p>\n", resp->status, resp->statustext->s);
        lk_buffer_append(resp->body, html_error_end, strlen(html_error_end));
        return;
    }
    if (!strcmp(method, "GET")) {
        // /littlekitten sample page
        if (!strcmp(uri, "/littlekitten")) {
            lk_httpresponse_add_header(resp, "Content-Type", "text/html");
            lk_buffer_append(resp->body, html_sample, strlen(html_sample));
            return;
        }

        LKString *content_type = lk_string_new("");
        lk_string_sprintf(content_type, "text/%s;", fileext(uri));
        lk_httpresponse_add_header(resp, "Content-Type", content_type->s);
        lk_string_free(content_type);

        // if no page given, try opening index.html, ...
        //$$ Better way to do this?
        if (!strcmp(uri, "/")) {
            z = read_uri_file(settings->home_dir->s, "/index.html", resp->body);
            if (z == -1) {
                z = read_uri_file(settings->home_dir->s, "/index.htm", resp->body);
                if (z == -1) {
                    z = read_uri_file(settings->home_dir->s, "/default.html", resp->body);
                    if (z == -1) {
                        z = read_uri_file(settings->home_dir->s, "/default.htm", resp->body);
                    }
                }
            }
        } else {
            z = read_uri_file(settings->home_dir->s, uri, resp->body);
        }

        if (z == -1) {
            // uri file not found
            resp->status = 404;
            lk_string_sprintf(resp->statustext, "File not found '%s'", uri);
            lk_httpresponse_add_header(resp, "Content-Type", "text/plain");
            lk_buffer_append_sprintf(resp->body, "File not found '%s'\n", uri);
        }
        return;
    }
    if (!strcmp(method, "POST")) {
        lk_httpresponse_add_header(resp, "Content-Type", "text/html");
        lk_buffer_append(resp->body, html_start, strlen(html_start));
        lk_buffer_append_sz(resp->body, "<pre>\n");
        lk_buffer_append(resp->body, req->body->bytes, req->body->bytes_len);
        lk_buffer_append_sz(resp->body, "\n</pre>\n");
        lk_buffer_append(resp->body, html_end, strlen(html_end));
        return;
    }
}

// Read <home_dir>/<uri> file into buffer.
// Returns 0 for success, -1 for error.
int read_uri_file(char *home_dir, char *uri, LKBuffer *buf) {
    // uri_path = home_dir + uri
    // Ex. "/path/to" + "/index.html"
    LKString *uri_path = lk_string_new(home_dir);
    lk_string_append(uri_path, uri);
    int z = lk_readfile(uri_path->s, buf);
    lk_string_free(uri_path);
    return z;
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

