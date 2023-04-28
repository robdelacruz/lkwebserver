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
void parse_args(int argc, char *argv[], LKConfig *cfg);

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

    LKConfig *cfg = lk_config_new();
    parse_args(argc, argv, cfg);
    LKHttpServer *httpserver = lk_httpserver_new(cfg);

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

typedef enum {PA_NONE, PA_FILE} ParseArgsState;

// lkws [homedir] [port] [host] [--cgidir=<cgidir>] [-f <configfile>]
// homedir    = absolute or relative path to a home directory
//              defaults to current working directory if not specified
// port       = port number to bind to server
//              defaults to 8000
// host       = IP address to bind to server
//              defaults to localhost
// cgidir     = root directory for cgi files, relative path to homedir
//              defaults to cgi-bin
// configfile = plaintext file containing configuration options
//
// Examples:
// lkws ./testsite/ 8080
// lkws /var/www/testsite/ 8080 127.0.0.1
// lkws /var/www/testsite/ --cgidir=guestbook
// lkws -f littlekitten.conf
void parse_args(int argc, char *argv[], LKConfig *cfg) {
    ParseArgsState state = PA_NONE;
    int is_homedir_set = 0;
    int is_port_set = 0;
    int is_host_set = 0;

    for (int i=1; i < argc; i++) {
        char *arg = argv[i];
        if (state == PA_NONE && !strcmp(arg, "-f")) {
            state = PA_FILE;
            continue;
        }
        // --cgidir=cgifolder
        if (state == PA_NONE && !strncmp(arg, "--cgidir=", 9)) {
            LKHostConfig *default_hc = lk_config_create_get_hostconfig(cfg, "*");
            LKString *v = lk_string_new("");
            sz_string_split_assign(arg, "=", NULL, v);
            lk_string_assign(default_hc->cgidir, v->s);
            lk_string_free(v);
            continue;
        }
        if (state == PA_FILE) {
            lk_config_read_configfile(cfg, arg);
            state = PA_NONE;
            continue;
        }
        assert(state == PA_NONE);
        if (!is_homedir_set) {
            LKHostConfig *default_hc = lk_config_create_get_hostconfig(cfg, "*");
            lk_string_assign(default_hc->homedir, arg);
            is_homedir_set = 1;
        } else if (!is_port_set) {
            lk_string_assign(cfg->port, arg);
            is_port_set = 1;
        } else if (!is_host_set) {
            lk_string_assign(cfg->serverhost, arg);
            is_host_set = 1;
        }
    }
}

