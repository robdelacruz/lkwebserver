#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "lklib.h"
#include "lknet.h"

int file_exists(char *filename) {
    struct stat statbuf;
    int z = stat(filename, &statbuf);
    return !z;
}

#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <cgifile>\n", argv[0]);
        exit(1);
    }
    char *cgifile = argv[1];
    if (!file_exists(cgifile)) {
        printf("%s doesn't exist.\n", cgifile);
        exit(1);
    }
    FILE *f1 = popen(cgifile, "r");
    if (f1 == NULL) {
        perror("popen()");
        exit(1);
    }

    int fd = fileno(f1);
    if (fd == -1) {
        perror("fileno()");
        exit(1);
    }

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    fcntl(fd, F_SETFL, O_NONBLOCK);
    char buf[BUF_SIZE];
    unsigned int j = 0;
    while (1) {
        fd_set cur_readfds = readfds;
        int z = select(fd+1, &cur_readfds, NULL, NULL, NULL);
        if (z == -1) {
            perror("select()");
            exit(1);
        }
        if (z == 0) {
            // timeout
            continue;
        }
        if (FD_ISSET(fd, &cur_readfds)) {
            z = read(fd, buf, sizeof(buf)-1);
            if (z == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                // no data for now, retry again
                printf("%c\033[1D", j ? '/' : '\\');
                j++;
                j = j % 2;
                continue;
            }
            if (z == -1 && errno == EINTR) {
                printf("\n*** EINTR ***\n");
                continue;
            }
            if (z == -1) {
                break;
            }
            if (z == 0) {
                // eof
                FD_CLR(fd, &readfds);
                break;
            }
            buf[z] = '\0';
            printf("%s", buf);
        }
    }

    pclose(f1);
    return 0;
}


