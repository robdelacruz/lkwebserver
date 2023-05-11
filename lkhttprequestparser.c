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

void parse_line(LKHttpRequestParser *parser, char *line, LKHttpRequest *req);
void parse_request_line(char *line, LKHttpRequest *req);
static void parse_header_line(LKHttpRequestParser *parser, char *line, LKHttpRequest *req);
void parse_uri(LKString *lks_uri, LKString *lks_path, LKString *lks_filename, LKString *lks_qs);

/*** LKHttpRequestParser functions ***/
LKHttpRequestParser *lk_httprequestparser_new() {
    LKHttpRequestParser *parser = lk_malloc(sizeof(LKHttpRequestParser), "lk_httprequest_parser_new");
    parser->partial_line = lk_string_new("");
    parser->nlinesread = 0;
    parser->content_length = 0;
    parser->head_complete = 0;
    parser->body_complete = 0;
    return parser;
}

void lk_httprequestparser_free(LKHttpRequestParser *parser) {
    lk_string_free(parser->partial_line);
    parser->partial_line = NULL;
    lk_free(parser);
}

// Clear any pending state.
void lk_httprequestparser_reset(LKHttpRequestParser *parser) {
    lk_string_assign(parser->partial_line, "");
    parser->nlinesread = 0;
    parser->content_length = 0;
    parser->head_complete = 0;
    parser->body_complete = 0;
}

// Parse one line and cumulatively compile results into req.
// You can check the state of the parser through the following fields:
// parser->head_complete   Request Line and Headers complete
// parser->body_complete   httprequest is complete
void lk_httprequestparser_parse_line(LKHttpRequestParser *parser, LKString *line, LKHttpRequest *req) {
    // If there's a previous partial line, combine it with current line.
    if (parser->partial_line->s_len > 0) {
        lk_string_append(parser->partial_line, line->s);
        if (lk_string_ends_with(parser->partial_line, "\n")) {
            parse_line(parser, parser->partial_line->s, req);
            lk_string_assign(parser->partial_line, "");
        }
        return;
    }

    // If current line is only partial line (not newline terminated), remember it for
    // next read.
    if (!lk_string_ends_with(line, "\n")) {
        lk_string_assign(parser->partial_line, line->s);
        return;
    }

    parse_line(parser, line->s, req);
}

void parse_line(LKHttpRequestParser *parser, char *line, LKHttpRequest *req) {
    // First line: parse initial request line.
    if (parser->nlinesread == 0) {
        parse_request_line(line, req);
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
        parse_header_line(parser, line, req);
        return;
    }
}

// Parse initial request line in the format:
// GET /path/to/index.html HTTP/1.0
void parse_request_line(char *line, LKHttpRequest *req) {
    char *toks[3];
    int ntoksread = 0;

    char *saveptr;
    char *delim = " \t";
    char *linetmp = lk_strdup(line, "parse_request_line");
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

    parse_uri(req->uri, req->path, req->filename, req->querystring);

    lk_free(linetmp);
}

// Parse uri into its components.
// Given: lks_uri   = "/path/blog/file1.html?a=1&b=2"
// lks_path         = "/path/blog/file1.html"
// lks_filename     = "file1.html"
// lks_qs           = "a=1&b=2"
void parse_uri(LKString *lks_uri, LKString *lks_path, LKString *lks_filename, LKString *lks_qs) { 
    // Get path and querystring
    // "/path/blog/file1.html?a=1&b=2" ==> "/path/blog/file1.html" and "a=1&b=2"
    LKStringList *uri_ss = lk_string_split(lks_uri, "?");
    lk_string_assign(lks_path, uri_ss->items[0]->s);
    if (uri_ss->items_len > 1) {
        lk_string_assign(lks_qs, uri_ss->items[1]->s);
    } else {
        lk_string_assign(lks_qs, "");
    }

    // Remove any trailing slash from uri. "/path/blog/" ==> "/path/blog"
    lk_string_chop_end(lks_path, "/");

    // Extract filename from path. "/path/blog/file1.html" ==> "file1.html"
    LKStringList *path_ss = lk_string_split(lks_path, "/");
    assert(path_ss->items_len > 0);
    lk_string_assign(lks_filename, path_ss->items[path_ss->items_len-1]->s);

    lk_stringlist_free(uri_ss);
    lk_stringlist_free(path_ss);
}


// Parse header line in the format Ex. User-Agent: browser
static void parse_header_line(LKHttpRequestParser *parser, char *line, LKHttpRequest *req) {
    char *saveptr;
    char *delim = ":";

    char *linetmp = lk_strdup(line, "parse_header_line");
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
    lk_httprequest_add_header(req, k, v);

    if (!strcasecmp(k, "Content-Length")) {
        int content_length = atoi(v);
        parser->content_length = content_length;
    }

    lk_free(linetmp);
}


// Parse sequence of bytes into request body. Compile results into req.
// You can check the state of the parser through the following fields:
// parser->head_complete   Request Line and Headers complete
// parser->body_complete   httprequest is complete
void lk_httprequestparser_parse_bytes(LKHttpRequestParser *parser, LKBuffer *buf, LKHttpRequest *req) {
    // Head should be parsed line by line. Call parse_line() instead.
    if (!parser->head_complete) {
        return;
    }
    if (parser->body_complete) {
        return;
    }

    lk_buffer_append(req->body, buf->bytes, buf->bytes_len);
    if (req->body->bytes_len >= parser->content_length) {
        parser->body_complete = 1;
    }
}

