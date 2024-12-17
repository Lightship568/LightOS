#include <lib/stdio.h>
#include <lib/syscall.h>
#include <lightos/fs.h>
#include <sys/types.h>

int main(int argc, char const* argv[]) {
    if (argc < 2) {
        printf("no filename input.\n");
        return EOF;
    }

    int fd = open((char*)argv[1], O_CREAT, 0755);
    if (fd < 0) {
        printf("touch failed with return %d\n", fd);
        return fd;
    }

    close(fd);
}