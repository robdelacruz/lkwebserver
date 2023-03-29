#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "lklib.h"

// Print the last error message corresponding to errno.
void lk_print_err(char *s) {
    fprintf(stderr, "%s: %s\n", s, strerror(errno));
}

void lk_exit_err(char *s) {
    lk_print_err(s);
    exit(1);
}


