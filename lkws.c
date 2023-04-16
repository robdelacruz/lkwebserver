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

void handle_sigint(int sig);
void handle_sigchld(int sig);
void parse_args(int argc, char *argv[], LKHttpServer *server);
void parse_args_alias(char *arg, LKHttpServer *server);

// lkws [homedir] [port] [host] [-a <alias1>=<path>]...
// homedir = absolute or relative path to a home directory
//           defaults to current working directory if not specified
// port    = port number to bind to server
//           defaults to 8000
// host    = IP address to bind to server
//           defaults to localhost
// Examples:
// lkws ./testsite/ 8080 -a latest=latest.html -a about=about.html
// lkws /var/www/testsite/ 8080 127.0.0.1 -a latest=folder/latest.html
// lkws /var/www/testsite/ --cgidir=cgifolder
int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);           // Don't abort on SIGPIPE
    signal(SIGINT, handle_sigint);      // exit on CTRL-C
    signal(SIGCHLD, handle_sigchld);

    LKHttpServer *httpserver = lk_httpserver_new();
    parse_args(argc, argv, httpserver);

    int z = lk_httpserver_serve(httpserver);
    if (z == -1) {
        return z;
    }

    // Code doesn't reach here.
    lk_httpserver_free(httpserver);
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

// lkws [homedir] [port] [host] [-a <alias1>=<path>]...
// homedir = absolute or relative path to a home directory
//           defaults to current working directory if not specified
// port    = port number to bind to server
//           defaults to 8000
// host    = IP address to bind to server
//           defaults to localhost
// Examples:
// lkws ./testsite/ 8080 -a latest=latest.html -a about=about.html
// lkws /var/www/testsite/ 8080 127.0.0.1 -a latest=folder/latest.html
// lkws /var/www/testsite/ --cgidir=cgifolder
void parse_args(int argc, char *argv[], LKHttpServer *server) {
    ParseArgsState state = PA_NONE;
    int is_homedir_set = 0;
    int is_port_set = 0;
    int is_host_set = 0;

    for (int i=1; i < argc; i++) {
        char *arg = argv[i];
        if (state == PA_NONE && !strcmp(arg, "-a")) {
            state = PA_ALIAS;
            continue;
        }
        // --cgidir=cgifolder
        if (state == PA_NONE && !strncmp(arg, "--cgidir=", 9)) {
            LKString *lksarg = lk_string_new(arg);
            LKStringList *parts = lk_string_split(lksarg, "=");
            if (parts->items_len == 2) {
                LKString *v = lk_stringlist_get(parts, 1);
                lk_httpserver_setopt(server, LKHTTPSERVEROPT_CGIDIR, v->s);
            }
            lk_stringlist_free(parts);
            lk_string_free(lksarg);
            continue;
        }
        if (state == PA_ALIAS) {
            parse_args_alias(arg, server);
            state = PA_NONE;
            continue;
        }
        assert(state == PA_NONE);
        if (!is_homedir_set) {
            lk_httpserver_setopt(server, LKHTTPSERVEROPT_HOMEDIR, arg);
            is_homedir_set = 1;
        } else if (!is_port_set) {
            lk_httpserver_setopt(server, LKHTTPSERVEROPT_PORT, arg);
            is_port_set = 1;
        } else if (!is_host_set) {
            lk_httpserver_setopt(server, LKHTTPSERVEROPT_HOST, arg);
            is_host_set = 1;
        }
    }
}

// Parse and add new alias definition into aliases
// alias definition "/latest=/latest.html" ==> "/latest" : "/latest.html"
void parse_args_alias(char *arg, LKHttpServer *server) {
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
        lk_httpserver_setopt(server, LKHTTPSERVEROPT_ALIAS, k->s, v->s);
    }

    lk_stringlist_free(parts);
    lk_string_free(lksarg);
}

