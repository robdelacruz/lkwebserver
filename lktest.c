#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "lklib.h"

void lkstr_test();
void lkstringmap_test();
void lkbuf_test();

int main(int argc, char *argv[]) {
    lkstr_test();
    lkstringmap_test();
    lkbuf_test();
    return 0;
}

void lkstr_test() {
    lkstr_s *lks, *lks2;

    printf("Running lkstr_s tests... ");
    lks = lkstr_new("");
    assert(lks->s_len == 0);
    assert(lks->s_size >= lks->s_len);
    assert(strcmp(lks->s, "") == 0);
    lkstr_free(lks);

    lks = lkstr_size_new(10);
    assert(lks->s_len == 0);
    assert(lks->s_size == 10);
    assert(strcmp(lks->s, "") == 0);
    lkstr_free(lks);

    // lks, lks2
    lks = lkstr_new("abc def");
    lks2 = lkstr_new("abc ");
    assert(lks->s_len == 7);
    assert(lks->s_size >= lks->s_len);
    assert(!lkstr_equal(lks, lks2));
    assert(strcmp(lks->s, "abc "));
    assert(strcmp(lks->s, "abc def2"));
    assert(!strcmp(lks->s, "abc def"));

    lkstr_append(lks2, "def");
    assert(!strcmp(lks->s, lks2->s));
    assert(lkstr_equal(lks, lks2));

    lkstr_assign(lks2, "abc def");
    assert(lkstr_equal(lks, lks2));

    lkstr_assign(lks2, "");
    assert(!lkstr_equal(lks, lks2));

    lkstr_sprintf(lks2, "abc %s", "def");
    assert(!strcmp(lks->s, lks2->s));
    assert(lkstr_equal(lks, lks2));

    lkstr_sprintf(lks, "abc%ddef%d", 123, 456);
    lkstr_sprintf(lks2, "%s123%s456", "abc", "def");
    assert(!strcmp(lks->s, lks2->s));
    assert(lkstr_equal(lks, lks2));
    lkstr_free(lks);
    lkstr_free(lks2);
    printf("Done.\n");
}

void lkstringmap_test() {
    lkstringmap_s *sm;
    void *v;

    printf("Running lkstringmap_s tests... ");
    sm = lkstringmap_funcs_new(free);
    assert(sm->items_len == 0);

    lkstringmap_set(sm, "abc", strdup("ABC"));
    lkstringmap_set(sm, "def", strdup("DEF"));
    assert(sm->items_len == 2);
    lkstringmap_set(sm, "123", strdup("one two three"));
    lkstringmap_set(sm, "  ", strdup("space space"));
    lkstringmap_set(sm, "", strdup("(blank)"));
    assert(sm->items_len == 5);

    v = lkstringmap_get(sm, "abc");
    assert(!strcmp(v, "ABC"));

    lkstringmap_set(sm, "abc", strdup("A B C"));
    v = lkstringmap_get(sm, "abc");
    assert(sm->items_len == 5);
    assert(strcmp(v, "ABC"));
    assert(!strcmp(v, "A B C"));

    lkstringmap_set(sm, "abc ", strdup("ABC(space)"));
    assert(sm->items_len == 6);
    v = lkstringmap_get(sm, "abc ");
    assert(!strcmp(v, "ABC(space)"));
    v = lkstringmap_get(sm, "abc");
    assert(!strcmp(v, "A B C"));

    v = lkstringmap_get(sm, "ABC");
    assert(v == NULL);
    v = lkstringmap_get(sm, "123");
    assert(!strcmp(v, "one two three"));
    v = lkstringmap_get(sm, "  ");
    assert(!strcmp(v, "space space"));
    v = lkstringmap_get(sm, " ");
    assert(v == NULL);
    v = lkstringmap_get(sm, "");
    assert(!strcmp(v, "(blank)"));

    lkstringmap_free(sm);
    printf("Done.\n");
}

void lkbuf_test() {
    lkbuf_s *buf;

    printf("Running lkbuf_s tests... ");
    buf = lkbuf_new(0);
    assert(buf->bytes_size > 0);
    lkbuf_free(buf);

    buf = lkbuf_new(10);
    assert(buf->bytes_size == 10);
    lkbuf_append(buf, "1234567", 7);
    assert(buf->bytes_len == 7);
    assert(buf->bytes_size == 10);
    assert(buf->bytes[0] == '1');
    assert(buf->bytes[1] == '2');
    assert(buf->bytes[2] == '3');
    assert(buf->bytes[5] == '6');

    lkbuf_append(buf, "890", 3);
    assert(buf->bytes_len == 10);
    assert(buf->bytes_size == 10);
    assert(buf->bytes[7] == '8');
    assert(buf->bytes[8] == '9');
    assert(buf->bytes[9] == '0');
    lkbuf_free(buf);

    buf = lkbuf_new(0);
    lkbuf_append(buf, "12345", 5);
    assert(buf->bytes_len == 5);
    assert(!strncmp(buf->bytes+0, "12345", 5));
    lkbuf_sprintf(buf, "abc %d def %d", 123, 456);
    assert(buf->bytes_len == 20);
    assert(!strncmp(buf->bytes+5, "abc 123", 7));
    assert(!strncmp(buf->bytes+5+8, "def 456", 7));
    lkbuf_free(buf);

    buf = lkbuf_new(0);
    lkbuf_append(buf, "12345", 5);
    assert(buf->bytes_len == 5);
    assert(!strncmp(buf->bytes+0, "12345", 5));
    lkbuf_asprintf(buf, "abc %d def %d", 123, 456);
    assert(buf->bytes_len == 20);
    assert(!strncmp(buf->bytes+5, "abc 123", 7));
    assert(!strncmp(buf->bytes+5+8, "def 456", 7));
    lkbuf_free(buf);

    printf("Done.\n");
}

