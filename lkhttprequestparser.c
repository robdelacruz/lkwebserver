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

void parse_line(LKHttpRequestParser *parser, char *line);
void parse_request_line(char *line, LKHttpRequest *req);
static void parse_header_line(LKHttpRequestParser *parser, char *line, LKHttpRequest *req);
int is_empty_line(char *s);
int ends_with_newline(char *s);

/*** LKHttpRequestParser functions ***/
LKHttpRequestParser *lk_httprequestparser_new(LKHttpRequest *req) {
    LKHttpRequestParser *parser = malloc(sizeof(LKHttpRequestParser));
    parser->partial_line = lk_string_new("");
    parser->nlinesread = 0;
    parser->content_length = 0;
    parser->head_complete = 0;
    parser->body_complete = 0;
    parser->req = req;
    return parser;
}

void lk_httprequestparser_free(LKHttpRequestParser *parser) {
    lk_string_free(parser->partial_line);
    parser->partial_line = NULL;
    parser->req = NULL;
    free(parser);
}

// Clear any pending state.
void lk_httprequestparser_reset(LKHttpRequestParser *parser) {
    lk_string_assign(parser->partial_line, "");
    parser->nlinesread = 0;
    parser->content_length = 0;
    parser->head_complete = 0;
    parser->body_complete = 0;
}

// Parse one line and cumulatively compile results into parser->req.
// You can check the state of the parser through the following fields:
// parser->head_complete   Request Line and Headers complete
// parser->body_complete   httprequest is complete
void lk_httprequestparser_parse_line(LKHttpRequestParser *parser, char *line) {
    // If there's a previous partial line, combine it with current line.
    if (parser->partial_line->s_len > 0) {
        lk_string_append(parser->partial_line, line);
        if (ends_with_newline(parser->partial_line->s)) {
            parse_line(parser, parser->partial_line->s);
            lk_string_assign(parser->partial_line, "");
        }
        return;
    }

    // If current line is only partial line (not newline terminated), remember it for
    // next read.
    if (!ends_with_newline(line)) {
        lk_string_assign(parser->partial_line, line);
        return;
    }

    parse_line(parser, line);
}

void parse_line(LKHttpRequestParser *parser, char *line) {
    // First line: parse initial request line.
    if (parser->nlinesread == 0) {
        parse_request_line(line, parser->req);
        parser->nlinesread++;
        return;
    }

    parser->nlinesread++;

    // Header lines
    if (!parser->head_complete) {
        // Empty CRLF line ends the headers section
        if (is_empty_line(line)) {
            parser->head_complete = 1;

            // No body to read (Content-Length: 0)
            if (parser->content_length == 0) {
                parser->body_complete = 1;
            }
            return;
        }
        parse_header_line(parser, line, parser->req);
        return;
    }
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

// Return whether string ends with \n char.
int ends_with_newline(char *s) {
    int slen = strlen(s);
    if (slen == 0) {
        return 0;
    }
    if (s[slen-1] == '\n') {
        return 1;
    }
    return 0;
}

// Parse initial request line in the format:
// GET /path/to/index.html HTTP/1.0
void parse_request_line(char *line, LKHttpRequest *req) {
    char *toks[3];
    int ntoksread = 0;

    char *saveptr;
    char *delim = " \t";
    char *linetmp = strdup(line);
    lk_chomp(linetmp);
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

    lk_string_assign(req->method, method);
    lk_string_assign(req->uri, uri);
    lk_string_assign(req->version, version);

    free(linetmp);
}

// Parse header line in the format Ex. User-Agent: browser
static void parse_header_line(LKHttpRequestParser *parser, char *line, LKHttpRequest *req) {
    char *saveptr;
    char *delim = ":";

    char *linetmp = strdup(line);
    lk_chomp(linetmp);
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
    lk_httprequest_add_header(req, k, v);

    if (!strcasecmp(k, "Content-Length")) {
        int content_length = atoi(v);
        parser->content_length = content_length;
    }

    free(linetmp);
}


// Parse sequence of bytes into request body. Compile results into parser->req.
// You can check the state of the parser through the following fields:
// parser->head_complete   Request Line and Headers complete
// parser->body_complete   httprequest is complete
void lk_httprequestparser_parse_bytes(LKHttpRequestParser *parser, char *buf, size_t buf_len) {
    // Head should be parsed line by line. Call parse_line() instead.
    if (!parser->head_complete) {
        return;
    }
    if (parser->body_complete) {
        return;
    }

    lk_buffer_append(parser->req->body, buf, buf_len);
    if (parser->req->body->bytes_len >= parser->content_length) {
        parser->body_complete = 1;
    }
}

