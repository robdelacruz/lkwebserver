#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "lklib.h"
#include "lknet.h"

void parse_cgi_header_line(char *line, LKHttpResponse *resp);

void parse_cgi_output(LKBuffer *buf, LKHttpResponse *resp) {
    char cgiline[LK_BUFSIZE_MEDIUM];
    int has_crlf = 0;

    // Parse cgi_outputbuf line by line into http response.
    while (buf->bytes_cur < buf->bytes_len) {
        lk_buffer_readline(buf, cgiline, sizeof(cgiline));
        lk_chomp(cgiline);

        // Empty CRLF line ends the headers section
        if (is_empty_line(cgiline)) {
            has_crlf = 1;
            break;
        }
        parse_cgi_header_line(cgiline, resp);
    }
    if (buf->bytes_cur < buf->bytes_len) {
        lk_buffer_append(
            resp->body,
            buf->bytes + buf->bytes_cur,
            buf->bytes_len - buf->bytes_cur
        );
    }

    char *content_type = lk_stringtable_get(resp->headers, "Content-Type");

    // If cgi error
    // copy cgi output as is to display the error messages.
    if (!has_crlf && content_type == NULL) {
        lk_stringtable_set(resp->headers, "Content-Type", "text/plain");
        lk_buffer_clear(resp->body);
        lk_buffer_append(resp->body, buf->bytes, buf->bytes_len);
    }
}

// Parse header line in the format Ex. User-Agent: browser
void parse_cgi_header_line(char *line, LKHttpResponse *resp) {
    char *saveptr;
    char *delim = ":";

    char *linetmp = lk_strdup(line, "parse_cgi_header_line");
    lk_chomp(linetmp);
    char *k = strtok_r(linetmp, delim, &saveptr);
    if (k == NULL) {
        lk_free(linetmp);
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
    lk_httpresponse_add_header(resp, k, v);

    if (!strcasecmp(k, "Status")) {
        int status = atoi(v);
        resp->status = status;
    }

    lk_free(linetmp);
}

