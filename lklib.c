#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "lklib.h"

// forward declarations
void close_pipes(int pair1[2], int pair2[2], int pair3[2]);

// Print the last error message corresponding to errno.
void lk_print_err(char *s) {
    fprintf(stderr, "%s: %s\n", s, strerror(errno));
}

void lk_exit_err(char *s) {
    lk_print_err(s);
    exit(1);
}

// Like popen() but with in, out, and err fd's.
int lk_popen3(char *cmd, int *fd_in, int *fd_out, int *fd_err) {
    int z;
    int in[2] = {0, 0};
    int out[2] = {0, 0};
    int err[2] = {0, 0};

    z = pipe(in);
    if (z == -1) {
        return z;
    }
    z = pipe(out);
    if (z == -1) {
        close_pipes(in, out, err);
        return z;
    }
    z = pipe(err);
    if (z == -1) {
        close_pipes(in, out, err);
        return z;
    }

    int pid = fork();
    if (pid == 0) {
        // child proc
        z = dup2(in[0], STDIN_FILENO);
        if (z == -1) {
            close_pipes(in, out, err);
            return z;
        }
        z = dup2(out[1], STDOUT_FILENO);
        if (z == -1) {
            close_pipes(in, out, err);
            return z;
        }
        z = dup2(err[1], STDERR_FILENO);
        if (z == -1) {
            close_pipes(in, out, err);
            return z;
        }

        close_pipes(in, out, err);
        z = execl("/bin/sh", "sh",  "-c", cmd, NULL);
        return z;
    }

    // parent proc
    if (fd_in != NULL) {
        *fd_in = in[1]; // return the other end of the dup2() pipe
    }
    close(in[0]);

    if (fd_out != NULL) {
        *fd_out = out[0];
    }
    close(out[1]);

    if (fd_err != NULL) {
        *fd_err = err[0];
    }
    close(err[1]);
    return 0;
}

void close_pipes(int pair1[2], int pair2[2], int pair3[2]) {
    int tmp_errno = errno;
    if (pair1[0] != 0) {
        close(pair1[0]);
    }
    if (pair1[1] != 0) {
        close(pair1[1]);
    }
    if (pair2[0] != 0) {
        close(pair2[0]);
    }
    if (pair2[1] != 0) {
        close(pair2[1]);
    }
    if (pair3[0] != 0) {
        close(pair3[0]);
    }
    if (pair3[1] != 0) {
        close(pair3[1]);
    }
    errno = tmp_errno;
    return;
}


