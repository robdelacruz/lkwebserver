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

int is_empty_line(char *s);
static void parse_header_line(LKHttpCGIParser *parser, char *line, LKHttpResponse *resp);

/*** LKHttpCGIParser functions ***/
LKHttpCGIParser *lk_httpcgiparser_new(LKHttpResponse *resp) {
    LKHttpCGIParser *parser = malloc(sizeof(LKHttpCGIParser));
    parser->head_complete = 0;
    parser->status = 200;
    parser->resp = resp;
    parser->resp->status = 200;
    return parser;
}

void lk_httpcgiparser_free(LKHttpCGIParser *parser) {
    parser->resp = NULL;
    free(parser);
}

// Clear any pending state.
void lk_httpcgiparser_reset(LKHttpCGIParser *parser) {
    parser->head_complete = 0;
    parser->status =  200;
    parser->resp->status = 200;
}

void lk_httpcgiparser_parse_line(LKHttpCGIParser *parser, char *line) {
    // Header lines
    if (!parser->head_complete) {
        // Empty CRLF line ends the headers section
        if (is_empty_line(line)) {
            parser->head_complete = 1;
            return;
        }
        parse_header_line(parser, line, parser->resp);
        return;
    }
}

void lk_httpcgiparser_parse_bytes(LKHttpCGIParser *parser, char *buf, size_t buf_len) {
    // Head should be parsed line by line. Call parse_line() instead.
    if (!parser->head_complete) {
        return;
    }
    lk_buffer_append(parser->resp->body, buf, buf_len);
}

// Parse header line in the format Ex. User-Agent: browser
static void parse_header_line(LKHttpCGIParser *parser, char *line, LKHttpResponse *resp) {
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
    lk_httpresponse_add_header(resp, k, v);

    if (!strcasecmp(k, "Status")) {
        int status = atoi(v);
        parser->status = status;
        parser->resp->status = status;
    }

    free(linetmp);
}


