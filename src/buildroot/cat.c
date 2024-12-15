#ifdef LIGHTOS
#include <lightos/fs.h>
#include <lib/stdio.h>
#include <lib/string.h>
#include <lib/syscall.h>
#include <sys/types.h>
#else
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#endif

#define BUFLEN 1024

char buf[BUFLEN];

int main(int argc, char const* argv[]) {
    if (argc < 2) {
        return EOF;
    }

    int fd = open((char*)argv[1], O_RDONLY, 0);
    if (fd < 0) {
        printf("file %s not exists.\n", argv[1]);
        return fd;
    }

    while (1) {
        int len = read(fd, buf, BUFLEN);
        if (len < 0) {
            break;
        }
        write(1, buf, len);
    }
    close(fd);
    return 0;
}
