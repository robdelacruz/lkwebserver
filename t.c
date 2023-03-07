#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "http.h"

// Print the last error message corresponding to errno.
void print_err(char *s) {
    fprintf(stderr, "%s: %s\n", s, strerror(errno));
}
void exit_err(char *s) {
    print_err(s);
    exit(1);
}

int main(int argc, char *argv[]) {
    httpreq_t *req = httpreq_new();

    char *getline = "GET /path/to/index.html HTTP/1.0";
    char *headerline1 = "User-Agent: rob";
    char *headerline2 = "From: abc@email.com";
    httpreq_parseline(req, getline);
    httpreq_parseline(req, headerline1);
    httpreq_parseline(req, headerline2);

    httpreq_debugprint(req);

    httpreq_free(req);
    return 0;
}


