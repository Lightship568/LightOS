#include <lib/stdio.h>
#include <lib/string.h>
#include <lib/syscall.h>
#include <lightos/fs.h>
#include <lightos/time.h>
#include <sys/types.h>

#define BUF_LEN 1024
#define EOK 0
#define MAX_CMD_LEN 256

static char buf[BUF_LEN];
static char cwd[MAX_PATH_LEN];

static void strftime(time_t time, char* buf) {
    // yyyy-mm-dd hh:mm:ss
    tm tm;
    localtime(time, &tm);
    char* ptr = buf;
    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon,
            tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

static void parsemode(int mode, char* buf) {
    memset(buf, '-', 10);
    buf[10] = '\0';
    char* ptr = buf;

    int mode_list[] = {IFREG, IFBLK, IFDIR, IFCHR, IFIFO, IFLNK, IFSOCK};
    char mode_char[] = {'-', 'b', 'd', 'c', 'p', 'l', 's'};

    int i = 0;
    for (; i < sizeof(mode_list) / sizeof(int); ++i) {
        if ((mode & IFMT) == mode_list[i]) {
            *ptr = mode_char[i];
            break;
        }
    }
    ptr++;
    for (i = 6; i >= 0; i -= 3) {
        int fmt = (mode >> i) & 07;
        if (fmt & 0b100) {
            *ptr = 'r';
        }
        ptr++;
        if (fmt & 0b010) {
            *ptr = 'w';
        }
        ptr++;
        if (fmt * 0b001) {
            *ptr = 'x';
        }
        ptr++;
    }
}

int main(int argc, char* argv[]) {
    bool is_list = false;
    bool is_pwd = true;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-l")) {
            is_list = true;
        } else {
            is_pwd = false;
        }
    }
    if (is_pwd) {
        getcwd(cwd, MAX_PATH_LEN);
        argv[argc++] = cwd; // 至少有一个 NULL 的盈余
    }
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            continue;
        }
        fd_t fd = open(argv[i], O_RDONLY, 0);
        int len = readdir(fd, buf, BUF_LEN);
        close(fd);
        if (len < 0) {
            printf("ls: error reading dir\n");
            continue;
        }
        bool is_first_print = true;
        for (int j = 0; j < (len / sizeof(dirent_t)); ++j) {
            dirent_t* dir = &((dirent_t*)buf)[j];
            if (!strcmp(dir->name, ".") || !strcmp(dir->name, "..")) {
                continue;
            }
            if (dir->nr == 0) {
                continue;
            }
            if (!is_list) {
                if (is_first_print) {
                    printf("%s", dir->name);
                    is_first_print = false;
                } else {
                    printf(" %s", dir->name);
                }
                continue;
            }

            stat_t statbuf;
            char pathstr[MAX_CMD_LEN + NAME_LEN];
            sprintf(pathstr, "%s/%s", argv[i], dir->name);
            if (stat(pathstr, &statbuf) < 0) {
                printf("ls: error checking stat of %s\n", dir->name);
                continue;
            }
            parsemode(statbuf.mode, buf);

            printf("%s ", buf);
            strftime(statbuf.mtime, buf);
            printf("% 2d %4d %4d %4d %s %s\n", statbuf.nlinks, statbuf.uid,
                   statbuf.gid, statbuf.size, buf, dir->name);
        }
        if (!is_list) {
            printf("\n");
        }
    }
    return 0;
}