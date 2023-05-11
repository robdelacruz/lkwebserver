#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "lklib.h"
#include "lknet.h"

void lkstring_test();
void lkstringmap_test();
void lkbuffer_test();
void lkstringlist_test();
void lkreflist_test();
void lkconfig_test();

int main(int argc, char *argv[]) {
    lk_alloc_init();

    lkstring_test();
    lkstringmap_test();
    lkbuffer_test();
    lkstringlist_test();
    lkreflist_test();
    lkconfig_test();

    lk_print_allocitems();

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

    lk_string_assign_sprintf(lks2, "abc %s", "def");
    assert(!strcmp(lks->s, lks2->s));
    assert(lk_string_equal(lks, lks2));

    lk_string_assign_sprintf(lks, "abc%ddef%d", 123, 456);
    lk_string_assign_sprintf(lks2, "%s123%s456", "abc", "def");
    assert(!strcmp(lks->s, lks2->s));
    assert(lk_string_equal(lks, lks2));
    lk_string_free(lks);
    lk_string_free(lks2);

    lks = lk_string_new("prompt: ");
    lk_string_append_sprintf(lks, "a:%d, b:%d, c:%d", 1, 2, 3);
    assert(lk_string_sz_equal(lks, "prompt: a:1, b:2, c:3"));
    assert(!lk_string_sz_equal(lks, "prompt: a: 1, b:2, c:3"));
    lk_string_free(lks);

    // Test very large strings.
    lks = lk_string_new("");
    char sbuf[LK_BUFSIZE_XXL];
    memset(sbuf, 'a', sizeof(sbuf));
    sbuf[sizeof(sbuf)-1] = '\0';
    lk_string_assign(lks, sbuf);

    assert(lks->s_len == sizeof(sbuf)-1);
    for (int i=0; i < sizeof(sbuf)-1; i++) {
        assert(lks->s[i] == 'a');
    }

    lk_string_assign_sprintf(lks, "sbuf: %s", sbuf);
    assert(lks->s_len == sizeof(sbuf)-1 + 6);
    assert(strncmp(lks->s, "sbuf: aaaaa", 11) == 0);
    assert(lks->s[lks->s_len-2] == 'a');
    assert(lks->s[lks->s_len-3] == 'a');
    assert(lks->s[lks->s_len-4] == 'a');
    lk_string_free(lks);

    lks = lk_string_new("abcdefghijkl mnopqrst");
    assert(lk_string_starts_with(lks, "abc"));
    assert(lk_string_starts_with(lks, "abcdefghijkl mnopqrst"));
    assert(!lk_string_starts_with(lks, "abcdefghijkl mnopqrstuvwxyz"));
    assert(!lk_string_starts_with(lks, "abcdefghijkl mnopqrst "));
    assert(lk_string_starts_with(lks, ""));

    assert(lk_string_ends_with(lks, "rst"));
    assert(lk_string_ends_with(lks, " mnopqrst"));
    assert(lk_string_ends_with(lks, "abcdefghijkl mnopqrst"));
    assert(!lk_string_ends_with(lks, " abcdefghijkl mnopqrst"));
    assert(lk_string_ends_with(lks, ""));

    lk_string_assign(lks, "");
    assert(lk_string_starts_with(lks, ""));
    assert(lk_string_ends_with(lks, ""));
    assert(!lk_string_starts_with(lks, "a"));
    assert(!lk_string_ends_with(lks, "a"));
    lk_string_free(lks);

    lks = lk_string_new("  abc  ");
    lk_string_trim(lks);
    assert(lk_string_sz_equal(lks, "abc"));

    lk_string_assign(lks, " \tabc \n  ");
    lk_string_trim(lks);
    assert(lk_string_sz_equal(lks, "abc"));

    lk_string_assign(lks, "   ");
    lk_string_trim(lks);
    assert(lk_string_sz_equal(lks, ""));
    lk_string_free(lks);

    lks = lk_string_new("/testsite/");
    lk_string_chop_start(lks, "abc");
    assert(lk_string_sz_equal(lks, "/testsite/"));
    lk_string_chop_start(lks, "");
    assert(lk_string_sz_equal(lks, "/testsite/"));
    lk_string_chop_start(lks, "///");
    assert(lk_string_sz_equal(lks, "/testsite/"));

    lk_string_chop_start(lks, "/");
    assert(lk_string_sz_equal(lks, "testsite/"));
    lk_string_chop_start(lks, "test");
    assert(lk_string_sz_equal(lks, "site/"));
    lk_string_chop_start(lks, "si1");
    assert(lk_string_sz_equal(lks, "site/"));

    lk_string_assign(lks, "/testsite/");
    lk_string_chop_end(lks, "abc");
    assert(lk_string_sz_equal(lks, "/testsite/"));
    lk_string_chop_end(lks, "");
    assert(lk_string_sz_equal(lks, "/testsite/"));
    lk_string_chop_end(lks, "///");
    assert(lk_string_sz_equal(lks, "/testsite/"));

    lk_string_chop_end(lks, "/");
    assert(lk_string_sz_equal(lks, "/testsite"));
    lk_string_chop_end(lks, "site");
    assert(lk_string_sz_equal(lks, "/test"));
    lk_string_chop_end(lks, "1st");
    assert(lk_string_sz_equal(lks, "/test"));

    lk_string_assign(lks, "");
    lk_string_chop_start(lks, "");
    assert(lk_string_sz_equal(lks, ""));
    lk_string_chop_end(lks, "");
    assert(lk_string_sz_equal(lks, ""));
    lk_string_chop_start(lks, "abc");
    assert(lk_string_sz_equal(lks, ""));
    lk_string_chop_end(lks, "def");
    assert(lk_string_sz_equal(lks, ""));
    lk_string_free(lks);

    lks = lk_string_new("abc");
    assert(lk_string_sz_equal(lks, "abc"));
    lk_string_append_char(lks, 'd');
    assert(lk_string_sz_equal(lks, "abcd"));
    lk_string_append_char(lks, 'e');
    lk_string_append_char(lks, 'f');
    lk_string_append_char(lks, 'g');
    assert(lk_string_sz_equal(lks, "abcdefg"));
    assert(lks->s_len == 7);
    lk_string_free(lks);

    lks = lk_string_new("abc");
    lk_string_prepend(lks, "def ");
    assert(lk_string_sz_equal(lks, "def abc"));
    lk_string_free(lks);

    lks = lk_string_size_new(25);
    lk_string_prepend(lks, " World");
    lk_string_prepend(lks, "Hello");
    lk_string_prepend(lks, "1. ");
    assert(lk_string_sz_equal(lks, "1. Hello World"));
    lk_string_free(lks);

    lks = lk_string_size_new(2);
    lk_string_assign(lks, "Little Kitten Webserver written in C");
    lk_string_prepend(lks, "A ");
    lk_string_append(lks, ".");
    assert(lk_string_sz_equal(lks, "A Little Kitten Webserver written in C."));
    lk_string_free(lks);

    lks = lk_string_new("");
    LKStringList *parts = lk_string_split(lks, "a");
    assert(parts->items_len == 1);
    assert(lk_string_sz_equal(parts->items[0], ""));
    lk_stringlist_free(parts);

    parts = lk_string_split(lks, "abc");
    assert(parts->items_len == 1);
    assert(lk_string_sz_equal(parts->items[0], ""));
    lk_stringlist_free(parts);

    parts = lk_string_split(lks, "");
    assert(parts->items_len == 1);
    assert(lk_string_sz_equal(parts->items[0], ""));
    lk_stringlist_free(parts);

    lk_string_assign(lks, "abc def ghijkl");
    parts = lk_string_split(lks, " ");
    assert(parts->items_len == 3);
    assert(lk_string_sz_equal(parts->items[0], "abc"));
    assert(lk_string_sz_equal(parts->items[1], "def"));
    assert(lk_string_sz_equal(parts->items[2], "ghijkl"));
    lk_stringlist_free(parts);
    parts = lk_string_split(lks, "--");
    assert(parts->items_len == 1);
    assert(lk_string_sz_equal(parts->items[0], "abc def ghijkl"));
    lk_stringlist_free(parts);

    lk_string_assign(lks, "12 little kitten 12 web server 12 written in C");
    parts = lk_string_split(lks, "12 ");
    assert(parts->items_len == 4);
    assert(lk_string_sz_equal(parts->items[0], ""));
    assert(lk_string_sz_equal(parts->items[1], "little kitten "));
    assert(lk_string_sz_equal(parts->items[2], "web server "));
    assert(lk_string_sz_equal(parts->items[3], "written in C"));
    lk_stringlist_free(parts);

    lk_string_assign(lks, "12 little kitten 12 web server 12 written in C 12 ");
    parts = lk_string_split(lks, "12 ");
    assert(parts->items_len == 5);
    assert(lk_string_sz_equal(parts->items[0], ""));
    assert(lk_string_sz_equal(parts->items[1], "little kitten "));
    assert(lk_string_sz_equal(parts->items[2], "web server "));
    assert(lk_string_sz_equal(parts->items[3], "written in C "));
    assert(lk_string_sz_equal(parts->items[4], ""));
    lk_stringlist_free(parts);
    lk_string_free(lks);

    printf("Done.\n");
}

void lkstringmap_test() {
    LKStringTable *st;
    void *v;

    printf("Running LKStringTable tests... ");
    st = lk_stringtable_new();
    assert(st->items_len == 0);

    lk_stringtable_set(st, "abc", "ABC");
    lk_stringtable_set(st, "def", "DEF");
    lk_stringtable_set(st, "ghi", "GHI");
    lk_stringtable_set(st, "hij", "HIJ");
    lk_stringtable_set(st, "klm", "KLM");
    assert(st->items_len == 5);

    lk_stringtable_remove(st, "ab");
    assert(st->items_len == 5);
    lk_stringtable_remove(st, "");
    assert(st->items_len == 5);
    lk_stringtable_remove(st, "ghi");
    assert(st->items_len == 4);
    lk_stringtable_remove(st, "klm");
    assert(st->items_len == 3);
    lk_stringtable_remove(st, "hij");
    lk_stringtable_remove(st, "hij");
    assert(st->items_len == 2);

    lk_stringtable_set(st, "123", "one two three");
    lk_stringtable_set(st, "  ", "space space");
    lk_stringtable_set(st, "", "(blank)");
    assert(st->items_len == 5);

    v = lk_stringtable_get(st, "abc");
    assert(!strcmp(v, "ABC"));

    lk_stringtable_set(st, "abc", "A B C");
    v = lk_stringtable_get(st, "abc");
    assert(st->items_len == 5);
    assert(strcmp(v, "ABC"));
    assert(!strcmp(v, "A B C"));

    lk_stringtable_set(st, "abc ", "ABC(space)");
    assert(st->items_len == 6);
    v = lk_stringtable_get(st, "abc ");
    assert(!strcmp(v, "ABC(space)"));
    v = lk_stringtable_get(st, "abc");
    assert(!strcmp(v, "A B C"));

    v = lk_stringtable_get(st, "ABC");
    assert(v == NULL);
    v = lk_stringtable_get(st, "123");
    assert(!strcmp(v, "one two three"));
    v = lk_stringtable_get(st, "  ");
    assert(!strcmp(v, "space space"));
    v = lk_stringtable_get(st, " ");
    assert(v == NULL);
    v = lk_stringtable_get(st, "");
    assert(!strcmp(v, "(blank)"));

    lk_stringtable_free(st);
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

    // Test buffer append on very large strings.
    buf = lk_buffer_new(0);
    char sbuf[LK_BUFSIZE_XXL];
    memset(sbuf, 'a', sizeof(sbuf));
    lk_buffer_append(buf, sbuf, sizeof(sbuf));

    assert(buf->bytes_len == sizeof(sbuf));
    for (int i=0; i < sizeof(sbuf); i++) {
        assert(buf->bytes[i] == 'a');
    }

    lk_buffer_free(buf);

    buf = lk_buffer_new(0);
    sbuf[sizeof(sbuf)-1] = '\0';
    lk_buffer_append_sprintf(buf, "buf: %s", sbuf);
    assert(buf->bytes_len == sizeof(sbuf)-1 + 5);
    assert(strncmp(buf->bytes, "buf: aaaaa", 10) == 0);
    assert(buf->bytes[buf->bytes_len-1] == 'a');
    assert(buf->bytes[buf->bytes_len-2] == 'a');
    assert(buf->bytes[buf->bytes_len-3] == 'a');
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
    LKString *lks123 = lk_string_new("123");
    lk_stringlist_append_lkstring(sl, lks123);
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

void lkreflist_test() {
    printf("Running LKRefList tests... ");

    LKString *abc = lk_string_new("abc");
    LKString *def = lk_string_new("def");
    LKString *ghi = lk_string_new("ghi");
    LKString *jkl = lk_string_new("jkl");
    LKString *mno = lk_string_new("mno");
    LKRefList *l = lk_reflist_new();
    lk_reflist_append(l, abc);
    lk_reflist_append(l, def);
    lk_reflist_append(l, ghi);
    lk_reflist_append(l, jkl);
    lk_reflist_append(l, mno);
    assert(l->items_len == 5);

    LKString *s = lk_reflist_get(l, 0);
    assert(lk_string_sz_equal(s, "abc"));
    s = lk_reflist_get(l, 5);
    assert(s == NULL);
    s = lk_reflist_get(l, 4);
    assert(lk_string_sz_equal(s, "mno"));
    s = lk_reflist_get(l, 3);
    assert(lk_string_sz_equal(s, "jkl"));
    lk_reflist_remove(l, 3);
    assert(l->items_len == 4);
    s = lk_reflist_get(l, 3);
    assert(lk_string_sz_equal(s, "mno"));

    lk_string_free(abc);
    lk_string_free(def);
    lk_string_free(ghi);
    lk_string_free(jkl);
    lk_string_free(mno);
    lk_reflist_free(l);
    printf("Done.\n");
}

void lkconfig_test() {
    printf("Running LKConfig tests... \n");

    LKConfig *cfg = lk_config_new();
    lk_config_read_configfile(cfg, "lktest.conf");
    assert(cfg != NULL);
    lk_config_print(cfg);
    lk_config_free(cfg);

    printf("Done.\n");
}

