#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "lklib.h"

lkstr_s *lkstr_new(char *s) {
}
lkstr_s *lkstr_size_new(size_t size) {
}
void lkstr_free(lkstr_s *lkstr) {
}

void lkstr_sprintf(lkstr_s *lkstr, char *fmt, ...) {
}
void lkstr_append(lkstr_s *lkstr, char *s) {
}

