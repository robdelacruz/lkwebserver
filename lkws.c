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

#include "lklib.h"
#include "lknet.h"

#define LISTEN_PORT "5000"

typedef struct {
    LKString    *home_dir;
    LKString    *cgi_dir;
    LKStringMap *aliases;
} FileHandlerSettings;

void serve_file_handler(void *handler_ctx, LKHttpRequest *req, LKHttpResponse *resp);
int read_uri_file(char *home_dir, char *uri, LKBuffer *buf);
int is_valid_http_method(char *method);
char *fileext(char *filepath);
void handle_sigint(int sig);
void handle_sigchld(int sig);
void parse_args(int argc, char *argv[], FileHandlerSettings *settings);
void parse_args_alias(char *arg, LKStringMap *aliases);

FileHandlerSettings *create_filehandler_settings();
void free_filehandler_settings(FileHandlerSettings *settings);

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
        lk_exit_err("getaddrinfo()");
    }

    int s0 = socket(servaddr->ai_family, servaddr->ai_socktype, servaddr->ai_protocol);
    if (s0 == -1) {
        lk_exit_err("socket()");
    }

    int yes=1;
    z = setsockopt(s0, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (s0 == -1) {
        lk_exit_err("setsockopt(SO_REUSEADDR)");
    }

    z = bind(s0, servaddr->ai_addr, servaddr->ai_addrlen);
    if (z != 0) {
        lk_exit_err("bind()");
    }

    // $ lkws /testsite -a latest=/latest.html -a about=/about.html
    FileHandlerSettings *settings = create_filehandler_settings();
    parse_args(argc, argv, settings);

    LKHttpServer *httpserver = lk_httpserver_new(serve_file_handler, (void*) settings);

    LKString *server_ipaddr_str = lk_get_ipaddr_string(servaddr->ai_addr);
    printf("Serving HTTP on %s port %s...\n", server_ipaddr_str->s, LISTEN_PORT);
    lk_string_free(server_ipaddr_str);
    freeaddrinfo(servaddr);

    z = lk_httpserver_serve(httpserver, s0);
    if (z == -1) {
        lk_exit_err("lk_httpserver_serve()");
    }

    // Code doesn't reach here.
    free_filehandler_settings(settings);
    lk_httpserver_free(httpserver);
    close(s0);
    return 0;
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

typedef enum {PA_NONE, PA_ALIAS} ParseArgsState;

// $ lkws testsite/ -a /latest=/latest.html -a about=/about.html
// $ lkws testsite/ -a latest=foo/latest.html -a about=about.html
void parse_args(int argc, char *argv[], FileHandlerSettings *settings) {
    LKString *home_dir = NULL;
    LKString *cgi_dir = NULL;
    ParseArgsState state = PA_NONE;

    for (int i=1; i < argc; i++) {
        char *arg = argv[i];
        if (state == PA_NONE && !strcmp(arg, "-a")) {
            state = PA_ALIAS;
            continue;
        }
        if (state == PA_ALIAS) {
            parse_args_alias(arg, settings->aliases);
            state = PA_NONE;
            continue;
        }
        assert(state == PA_NONE);
        if (home_dir == NULL) {
            home_dir = lk_string_new(arg);
            continue;
        }
    }

    if (home_dir) {
        lk_string_chop_end(home_dir, "/");
        settings->home_dir = lk_string_new(home_dir->s);
    }
    if (cgi_dir) {
        lk_string_chop_end(cgi_dir, "/");
        settings->cgi_dir = lk_string_new(cgi_dir->s);
    }
}

// Parse and add new alias definition into aliases
// alias definition "/latest=/latest.html" ==> "/latest" : "/latest.html"
void parse_args_alias(char *arg, LKStringMap *aliases) {
    LKString *lksarg = lk_string_new(arg);
    LKStringList *parts = lk_string_split(lksarg, "=");
    if (parts->items_len == 2) {
        LKString *k = lk_stringlist_get(parts, 0);
        LKString *v = lk_stringlist_get(parts, 1);

        if (!lk_string_starts_with(k, "/")) {
            lk_string_prepend(k, "/");
        }
        if (!lk_string_starts_with(v, "/")) {
            lk_string_prepend(v, "/");
        }
        lk_stringmap_set(aliases, k->s, lk_string_new(v->s));
    }

    lk_stringlist_free(parts);
    lk_string_free(lksarg);
}

FileHandlerSettings *create_filehandler_settings() {
    FileHandlerSettings *fhs = malloc(sizeof(FileHandlerSettings));
    memset(fhs, 0, sizeof(FileHandlerSettings));
    fhs->home_dir = lk_string_new("");
    fhs->cgi_dir = lk_string_new("");
    fhs->aliases = lk_stringmap_funcs_new(lk_string_voidp_free);
    return fhs;
}

void free_filehandler_settings(FileHandlerSettings *settings) {
    lk_string_free(settings->home_dir);
    lk_string_free(settings->cgi_dir);
    lk_stringmap_free(settings->aliases);
    free(settings);
}


// Generate an http response to an http request.
void serve_file_handler(void *handler_ctx, LKHttpRequest *req, LKHttpResponse *resp) {
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

#if 0
    static char *html_start =
       "<!DOCTYPE html>\n"
       "<html>\n"
       "<head><title>Little Kitten Sample Response</title></head>\n"
       "<body>\n";
    static char *html_end =
       "</body></html>\n";
#endif

    char *method = req->method->s;
    char *uri = req->uri->s;

    if (!strcmp(method, "GET")) {
        // /littlekitten sample page
        if (!strcmp(uri, "/littlekitten")) {
            lk_httpresponse_add_header(resp, "Content-Type", "text/html");
            lk_buffer_append(resp->body, html_sample, strlen(html_sample));
            return;
        }

        LKString *content_type = lk_string_new("");
        lk_string_assign_sprintf(content_type, "text/%s;", fileext(uri));
        lk_httpresponse_add_header(resp, "Content-Type", content_type->s);
        lk_string_free(content_type);

        // Use alias if there's a match for uri.
        // Ex. uri: "/latest" => "/latest.html"
        LKString *alias_uri = lk_stringmap_get(settings->aliases, uri);
        if (alias_uri != NULL) {
            uri = alias_uri->s;
        }

        // if no page given, try opening index.html, ...
        //$$ Better way to do this?
        if (!strcmp(uri, "/")) {
            while (1) {
                z = read_uri_file(settings->home_dir->s, "/index.html", resp->body);
                if (z == 0) break;
                z = read_uri_file(settings->home_dir->s, "/index.htm", resp->body);
                if (z == 0) break;
                z = read_uri_file(settings->home_dir->s, "/default.html", resp->body);
                if (z == 0) break;
                z = read_uri_file(settings->home_dir->s, "/default.htm", resp->body);
                break;
            }
        } else {
            z = read_uri_file(settings->home_dir->s, uri, resp->body);
        }
        if (z == -1) {
            // uri file not found
            resp->status = 404;
            lk_string_assign_sprintf(resp->statustext, "File not found '%s'", uri);
            lk_httpresponse_add_header(resp, "Content-Type", "text/plain");
            lk_buffer_append_sprintf(resp->body, "File not found '%s'\n", uri);
        }

        return;
    }
#if 0
    if (!strcmp(method, "POST")) {
        lk_httpresponse_add_header(resp, "Content-Type", "text/html");
        lk_buffer_append(resp->body, html_start, strlen(html_start));
        lk_buffer_append_sz(resp->body, "<pre>\n");
        lk_buffer_append(resp->body, req->body->bytes, req->body->bytes_len);
        lk_buffer_append_sz(resp->body, "\n</pre>\n");
        lk_buffer_append(resp->body, html_end, strlen(html_end));
        return;
    }
#endif

    resp->status = 501;
    lk_string_assign_sprintf(resp->statustext, "Unsupported method ('%s')", method);

    lk_buffer_append(resp->body, html_error_start, strlen(html_error_start));
    lk_buffer_append_sprintf(resp->body, "<p>Error code %d.</p>\n", resp->status);
    lk_buffer_append_sprintf(resp->body, "<p>Message: Unsupported method ('%s').</p>\n", resp->statustext->s);
    lk_buffer_append(resp->body, html_error_end, strlen(html_error_end));
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

