#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "lklib.h"

void lkstring_test();
void lkstringmap_test();
void lkbuffer_test();
void lkstringlist_test();

int main(int argc, char *argv[]) {
    lkstring_test();
    lkstringmap_test();
    lkbuffer_test();
    lkstringlist_test();
    return 0;
}

void lkstring_test() {
    LKString *lks, *lks2;

    printf("Running LKString tests... ");
    lks = lk_string_new("");
    assert(lks->s_len == 0);
    assert(lks->s_size >= lks->s_len);
    assert(strcmp(lks->s, "") == 0);
    lk_string_free(lks);

    lks = lk_string_size_new(10);
    assert(lks->s_len == 0);
    assert(lks->s_size == 10);
    assert(strcmp(lks->s, "") == 0);
    lk_string_free(lks);

    // lks, lks2
    lks = lk_string_new("abc def");
    lks2 = lk_string_new("abc ");
    assert(lks->s_len == 7);
    assert(lks->s_size >= lks->s_len);
    assert(!lk_string_equal(lks, lks2));
    assert(strcmp(lks->s, "abc "));
    assert(strcmp(lks->s, "abc def2"));
    assert(!strcmp(lks->s, "abc def"));

    lk_string_append(lks2, "def");
    assert(!strcmp(lks->s, lks2->s));
    assert(lk_string_equal(lks, lks2));

    lk_string_assign(lks2, "abc def");
    assert(lk_string_equal(lks, lks2));

    lk_string_assign(lks2, "");
    assert(!lk_string_equal(lks, lks2));

    lk_string_sprintf(lks2, "abc %s", "def");
    assert(!strcmp(lks->s, lks2->s));
    assert(lk_string_equal(lks, lks2));

    lk_string_sprintf(lks, "abc%ddef%d", 123, 456);
    lk_string_sprintf(lks2, "%s123%s456", "abc", "def");
    assert(!strcmp(lks->s, lks2->s));
    assert(lk_string_equal(lks, lks2));
    lk_string_free(lks);
    lk_string_free(lks2);
    printf("Done.\n");
}

void lkstringmap_test() {
    LKStringMap *sm;
    void *v;

    printf("Running LKStringMap tests... ");
    sm = lk_stringmap_funcs_new(free);
    assert(sm->items_len == 0);

    lk_stringmap_set(sm, "abc", strdup("ABC"));
    lk_stringmap_set(sm, "def", strdup("DEF"));
    lk_stringmap_set(sm, "ghi", strdup("GHI"));
    lk_stringmap_set(sm, "hij", strdup("HIJ"));
    lk_stringmap_set(sm, "klm", strdup("KLM"));
    assert(sm->items_len == 5);

    lk_stringmap_remove(sm, "ab");
    assert(sm->items_len == 5);
    lk_stringmap_remove(sm, "");
    assert(sm->items_len == 5);
    lk_stringmap_remove(sm, "ghi");
    assert(sm->items_len == 4);
    lk_stringmap_remove(sm, "klm");
    assert(sm->items_len == 3);
    lk_stringmap_remove(sm, "hij");
    lk_stringmap_remove(sm, "hij");
    assert(sm->items_len == 2);

    lk_stringmap_set(sm, "123", strdup("one two three"));
    lk_stringmap_set(sm, "  ", strdup("space space"));
    lk_stringmap_set(sm, "", strdup("(blank)"));
    assert(sm->items_len == 5);

    v = lk_stringmap_get(sm, "abc");
    assert(!strcmp(v, "ABC"));

    lk_stringmap_set(sm, "abc", strdup("A B C"));
    v = lk_stringmap_get(sm, "abc");
    assert(sm->items_len == 5);
    assert(strcmp(v, "ABC"));
    assert(!strcmp(v, "A B C"));

    lk_stringmap_set(sm, "abc ", strdup("ABC(space)"));
    assert(sm->items_len == 6);
    v = lk_stringmap_get(sm, "abc ");
    assert(!strcmp(v, "ABC(space)"));
    v = lk_stringmap_get(sm, "abc");
    assert(!strcmp(v, "A B C"));

    v = lk_stringmap_get(sm, "ABC");
    assert(v == NULL);
    v = lk_stringmap_get(sm, "123");
    assert(!strcmp(v, "one two three"));
    v = lk_stringmap_get(sm, "  ");
    assert(!strcmp(v, "space space"));
    v = lk_stringmap_get(sm, " ");
    assert(v == NULL);
    v = lk_stringmap_get(sm, "");
    assert(!strcmp(v, "(blank)"));

    lk_stringmap_free(sm);
    printf("Done.\n");
}

void lkbuffer_test() {
    LKBuffer *buf;

    printf("Running LKBuffer tests... ");
    buf = lk_buffer_new(0);
    assert(buf->bytes_size > 0);
    lk_buffer_free(buf);

    buf = lk_buffer_new(10);
    assert(buf->bytes_size == 10);
    lk_buffer_append(buf, "1234567", 7);
    assert(buf->bytes_len == 7);
    assert(buf->bytes_size == 10);
    assert(buf->bytes[0] == '1');
    assert(buf->bytes[1] == '2');
    assert(buf->bytes[2] == '3');
    assert(buf->bytes[5] == '6');

    lk_buffer_append(buf, "890", 3);
    assert(buf->bytes_len == 10);
    assert(buf->bytes_size == 10);
    assert(buf->bytes[7] == '8');
    assert(buf->bytes[8] == '9');
    assert(buf->bytes[9] == '0');
    lk_buffer_free(buf);

    buf = lk_buffer_new(0);
    lk_buffer_append(buf, "12345", 5);
    assert(buf->bytes_len == 5);
    assert(!strncmp(buf->bytes+0, "12345", 5));
    lk_buffer_append_sprintf(buf, "abc %d def %d", 123, 456);
    assert(buf->bytes_len == 20);
    assert(!strncmp(buf->bytes+5, "abc 123", 7));
    assert(!strncmp(buf->bytes+5+8, "def 456", 7));
    lk_buffer_free(buf);

    buf = lk_buffer_new(0);
    lk_buffer_append(buf, "12345", 5);
    assert(buf->bytes_len == 5);
    assert(!strncmp(buf->bytes+0, "12345", 5));
    lk_buffer_append_sprintf(buf, "abc %d def %d", 123, 456);
    assert(buf->bytes_len == 20);
    assert(!strncmp(buf->bytes+5, "abc 123", 7));
    assert(!strncmp(buf->bytes+5+8, "def 456", 7));
    lk_buffer_free(buf);

    printf("Done.\n");
}

void lkstringlist_test() {
    LKStringList *sl;

    printf("Running LKStringList tests... ");
    sl = lk_stringlist_new();
    assert(sl->items_len == 0);
    lk_stringlist_append(sl, "abc");
    lk_stringlist_append_sprintf(sl, "Hello %s", "abc");
    lk_stringlist_append(sl, "123");
    assert(sl->items_len == 3);

    LKString *s0 = lk_stringlist_get(sl, 0);
    LKString *s1 = lk_stringlist_get(sl, 1);
    LKString *s2 = lk_stringlist_get(sl, 2);
    LKString *s3 = lk_stringlist_get(sl, 3);
    assert(s0 != NULL);
    assert(s1 != NULL);
    assert(s2 != NULL);
    assert(s3 == NULL);
    assert(!strcmp(s0->s, "abc"));
    assert(!strcmp(s1->s, "Hello abc"));
    assert(!strcmp(s2->s, "123"));

    lk_stringlist_remove(sl, 0); // remove "abc"
    assert(sl->items_len == 2);
    s0 = lk_stringlist_get(sl, 0);
    s1 = lk_stringlist_get(sl, 1);
    s2 = lk_stringlist_get(sl, 2);
    assert(s0 != NULL);
    assert(s1 != NULL);
    assert(s2 == NULL);
    assert(!strcmp(s0->s, "Hello abc"));
    assert(!strcmp(s1->s, "123"));

    lk_stringlist_remove(sl, 1); // remove "123"
    assert(sl->items_len == 1);
    s0 = lk_stringlist_get(sl, 0);
    s1 = lk_stringlist_get(sl, 1);
    s2 = lk_stringlist_get(sl, 2);
    assert(s0 != NULL);
    assert(s1 == NULL);
    assert(s2 == NULL);
    assert(!strcmp(s0->s, "Hello abc"));

    lk_stringlist_free(sl);
    printf("Done.\n");
}

