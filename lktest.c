#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "lklib.h"

int main(int argc, char *argv[]) {
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

    return 0;
}

