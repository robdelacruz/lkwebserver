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

int main(int argc, char *argv[]) {
    int z;
    httpreq_t *req = httpreq_new();

    char *lines[] = {
        "GET /path/to/index.html HTTP/1.0\n",
        "User-Agent: rob\n",
        "From: abc@email.com\n",
        "header3: abc def ghi\n",
        "header4: 123 def\n",
        "header5: a\n",
        "header6: a\n",
        "header7: a\n",
        "header8: a\n",
        "header9: a\n",
        "header10 abc: a\n",
        "header11: a\n",
        "header12 abc 12345: a\n",
        "header13 a b c : a\n",
        "header14: a\n",
        "header15: a\n",
        "\r\n",
        "message body\n"
        "message body 2\n"
    };

    int empty_line_parsed = 0;

    for (int i=0; i < sizeof(lines) / sizeof(char *); i++) {
        // initial line
        if (i == 0) {
            if (httpreq_parse_request_line(req, lines[i]) == -1) {
                // If initial line is invalid, stop parsing.
                break;
            }
            continue;
        }

        if (is_empty_line(lines[i])) {
            empty_line_parsed = 1;
            continue;
        }

        if (!empty_line_parsed) {
            httpreq_parse_header_line(req, lines[i]);
            continue;
        }

        // parse message body
        httpreq_append_body(req, lines[i], strlen(lines[i]));
    }
    httpreq_debugprint(req);

    httpreq_free(req);
    return 0;
}


