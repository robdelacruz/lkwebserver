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
void lkstringlist_test();

int main(int argc, char *argv[]) {
    lkstr_test();
    lkstringmap_test();
    lkbuf_test();
    lkstringlist_test();
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
    lkstringmap_set(sm, "ghi", strdup("GHI"));
    lkstringmap_set(sm, "hij", strdup("HIJ"));
    lkstringmap_set(sm, "klm", strdup("KLM"));
    assert(sm->items_len == 5);

    lkstringmap_remove(sm, "ab");
    assert(sm->items_len == 5);
    lkstringmap_remove(sm, "");
    assert(sm->items_len == 5);
    lkstringmap_remove(sm, "ghi");
    assert(sm->items_len == 4);
    lkstringmap_remove(sm, "klm");
    assert(sm->items_len == 3);
    lkstringmap_remove(sm, "hij");
    lkstringmap_remove(sm, "hij");
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
    lkbuf_append_sprintf(buf, "abc %d def %d", 123, 456);
    assert(buf->bytes_len == 20);
    assert(!strncmp(buf->bytes+5, "abc 123", 7));
    assert(!strncmp(buf->bytes+5+8, "def 456", 7));
    lkbuf_free(buf);

    buf = lkbuf_new(0);
    lkbuf_append(buf, "12345", 5);
    assert(buf->bytes_len == 5);
    assert(!strncmp(buf->bytes+0, "12345", 5));
    lkbuf_append_sprintf(buf, "abc %d def %d", 123, 456);
    assert(buf->bytes_len == 20);
    assert(!strncmp(buf->bytes+5, "abc 123", 7));
    assert(!strncmp(buf->bytes+5+8, "def 456", 7));
    lkbuf_free(buf);

    printf("Done.\n");
}

void lkstringlist_test() {
    lkstringlist_s *sl;

    printf("Running lkstringlist_s tests... ");
    sl = lkstringlist_new();
    assert(sl->items_len == 0);
    lkstringlist_append(sl, "abc");
    lkstringlist_append_sprintf(sl, "Hello %s", "abc");
    lkstringlist_append(sl, "123");
    assert(sl->items_len == 3);

    lkstr_s *s0 = lkstringlist_get(sl, 0);
    lkstr_s *s1 = lkstringlist_get(sl, 1);
    lkstr_s *s2 = lkstringlist_get(sl, 2);
    lkstr_s *s3 = lkstringlist_get(sl, 3);
    assert(s0 != NULL);
    assert(s1 != NULL);
    assert(s2 != NULL);
    assert(s3 == NULL);
    assert(!strcmp(s0->s, "abc"));
    assert(!strcmp(s1->s, "Hello abc"));
    assert(!strcmp(s2->s, "123"));

    lkstringlist_remove(sl, 0); // remove "abc"
    assert(sl->items_len == 2);
    s0 = lkstringlist_get(sl, 0);
    s1 = lkstringlist_get(sl, 1);
    s2 = lkstringlist_get(sl, 2);
    assert(s0 != NULL);
    assert(s1 != NULL);
    assert(s2 == NULL);
    assert(!strcmp(s0->s, "Hello abc"));
    assert(!strcmp(s1->s, "123"));

    lkstringlist_remove(sl, 1); // remove "123"
    assert(sl->items_len == 1);
    s0 = lkstringlist_get(sl, 0);
    s1 = lkstringlist_get(sl, 1);
    s2 = lkstringlist_get(sl, 2);
    assert(s0 != NULL);
    assert(s1 == NULL);
    assert(s2 == NULL);
    assert(!strcmp(s0->s, "Hello abc"));

    lkstringlist_free(sl);
    printf("Done.\n");
}

