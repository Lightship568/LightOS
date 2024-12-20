#include <lib/stdlib.h>
#include <lib/string.h>
#include <lib/syscall.h>
#include <lib/vsprintf.h>
#include <lightos/fs.h>
#include <lightos/time.h>

#define MAX_CMD_LEN 256
#define MAX_ARG_NR 16
#define BUFLEN 1024

static char cwd[MAX_PATH_LEN];
static char cmd[MAX_CMD_LEN];
static char* argv[MAX_ARG_NR];
static char buf[BUFLEN];

static const char* lightos_logo =
    "\n\t"
    "\t /$$       /$$           /$$         /$$      /$$$$$$   /$$$$$$ \n"
    "\t| $$      |__/          | $$        | $$     /$$__  $$ /$$__  $$\n"
    "\t| $$       /$$  /$$$$$$ | $$$$$$$  /$$$$$$  | $$  \\ $$| $$  \\__/\n"
    "\t| $$      | $$ /$$__  $$| $$__  $$|_  $$_/  | $$  | $$|  $$$$$$ \n"
    "\t| $$      | $$| $$  \\ $$| $$  \\ $$  | $$    | $$  | $$ \\____  $$\n"
    "\t| $$      | $$| $$  | $$| $$  | $$  | $$ /$$| $$  | $$ /$$  \\ $$\n"
    "\t| $$$$$$$$| $$|  $$$$$$$| $$  | $$  |  $$$$/|  $$$$$$/|  $$$$$$/\n"
    "\t|________/|__/ \\____  $$|__/  |__/   \\___/   \\______/  \\______/ \n"
    "\t               /$$  \\ $$                                        \n"
    "\t              |  $$$$$$/                                        \n"
    "\t               \\______/                                         \n\0";

extern char* strsep(const char* str);
extern char* strrsep(const char* str);
extern int printf(const char* fmt, ...);

static char* envp[] = {
    "HOME=/",
    "PATH=/bin",
    NULL,
};

void print_prompt() {
    getcwd(cwd, MAX_PATH_LEN);
    char* ptr = strrsep(cwd);
    printf("[root %s]# ", ptr[1] ? ptr + 1 : ptr);
}

void builtin_logo() {
    // clear();
    printf((char*)lightos_logo);
}

void builtin_test(int argc, char* argv[]) {
    printf("lsh testing...\n");
    int status = 0;
    fd_t pipefd[2];

    int result = pipe(pipefd);
    char* message = "This is a pipe test message!";
    int len_message = strlen(message);

    pid_t pid = fork();
    if (pid) {
        char buf[128];
        memset(buf, 0, sizeof(buf));
        printf("[p-%d] getting message\n", getpid());
        int len = read(pipefd[0], buf, len_message);
        printf("[p-%d] get message: %s\n", getpid(), buf);
        printf("message count %d\n", len);

        pid_t child = waitpid(pid, &status, 0);
        close(pipefd[1]);
        close(pipefd[0]);
    } else {
        printf("[c-%d] put message: %s\n", getpid(), message);
        write(pipefd[1], message, len_message);

        close(pipefd[1]);
        close(pipefd[0]);
        exit(0);
    }
}

void readline(char* buf, u32 count) {
    if (!buf)
        return;
    char* ptr = buf;
    u32 idx = 0;
    char ch = 0;
    while (idx < count) {
        ptr = buf + idx;
        int ret = read(STDIN_FILENO, ptr, 1);
        if (ret == -1) {
            *ptr = 0;
            return;
        }
        switch (*ptr) {
            case '\n':
            case '\r':
                *ptr = 0;
                ch = '\n';
                write(STDOUT_FILENO, &ch, 1);
                return;
            case '\b':
                if (idx > 0) {
                    idx--;
                    ch = '\b';
                    write(STDOUT_FILENO, &ch, 1);
                }
                break;
            case '\t':
                continue;
            default:
                write(STDOUT_FILENO, ptr, 1);
                idx++;
                break;
        }
    }
    buf[idx] = '\0';
}

static int cmd_parse(char* cmd, char* argv[], char token) {
    if (!cmd)
        return 0;

    char* next = cmd;
    int argc = 0;
    while (*next && argc < MAX_ARG_NR) {
        while (*next == token) {
            next++;
        }
        if (*next == 0) {
            break;
        }
        argv[argc++] = next;
        while (*next && *next != token) {
            next++;
        }
        if (*next) {
            *next = 0;  // 将分隔符修改为 \0，作为 argv 的分割
            next++;
        }
    }
    argv[argc] = NULL;
    return argc;
}

static void builtin_pwd() {
    getcwd(cwd, MAX_PATH_LEN);
    printf("%s\n", cwd);
}

static void builtin_clear(void) {
    // todo
}

static void builtin_exit(int argc, char* argv[]) {
    int code = 0;
    if (argc == 2) {
        code = atoi(argv[1]);
    }
    exit(code);
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

void strftime(time_t time, char* buf) {
    // yyyy-mm-dd hh:mm:ss
    tm tm;
    localtime(time, &tm);
    char* ptr = buf;
    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon,
            tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

static void builtin_ls(int argc, char* argv[]) {
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
        argc++;
        argv[argc - 1] = cwd;
    }
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            continue;
        }
        fd_t fd = open(argv[i], O_RDONLY, 0);
        int len = readdir(fd, buf, BUFLEN);
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
}

static void builtin_cd(int argc, char* argv[]) {
    if (argc > 1) {
        if (chdir(argv[1]) < 0) {
            printf("cd: no such dir %s\n", argv[1]);
        };
    }
}
static void builtin_cat(int argc, char* argv[]) {
    if (argc < 2) {
        printf("cat: no input file\n");
        return;
    }
    for (int i = 1; i < argc; ++i) {
        fd_t fd = open(argv[i], O_RDONLY, 0);
        if (fd < 0) {
            printf("cat: no such file %s\n", argv[i]);
            continue;
        }
        while (true) {
            int len = read(fd, buf, 1);
            if (len == EOF) {
                break;
            }
            write(STDOUT_FILENO, buf, len);
        }
        close(fd);
    }
}
static void builtin_mkdir(int argc, char* argv[]) {
    if (argc < 2) {
        printf("mkdir: no input dir\n");
        return;
    }
    for (int i = 1; i < argc; ++i) {
        if (mkdir(argv[i], 0755) < 0) {
            printf("mkdir: error creating dir %s\n", argv[i]);
        }
    }
}
static void builtin_rmdir(int argc, char* argv[]) {
    if (argc < 2) {
        printf("rmdir: no input dir\n");
        return;
    }
    for (int i = 1; i < argc; ++i) {
        if (rmdir(argv[i]) < 0) {
            printf("rmdir: error removing dir %s\n", argv[i]);
        } else {
            printf("rmdir: %s\n", argv[i]);
        }
    }
}
static void builtin_rm(int argc, char* argv[]) {
    if (argc < 2) {
        printf("rm: no input dir\n");
        return;
    }
    for (int i = 1; i < argc; ++i) {
        unlink(argv[i]);
    }
}

static void builtin_date(void) {
    strftime(time(), buf);
    printf("%s\n", buf);
}

static void builtin_mount(int argc, char* argv[]) {
    if (argc < 3) {
        return;
    }
    if (mount(argv[1], argv[2], 0)) {
        printf("error mount %s to %s\n", argv[1], argv[2]);
    }
}

static void builtin_umount(int argc, char* argv[]) {
    if (argc < 2) {
        return;
    }
    if (umount(argv[1])) {
        printf("error umount %s\n", argv[1]);
    }
}

static void builtin_mkfs(int argc, char* argv[]) {
    if (argc < 3) {
        return;
    }
    if (mkfs(argv[1], atoi(argv[2]))) {
        printf("error mkfs %s\n", argv[1]);
    } else {
        printf("success\n");
    }
}

static void dupfile(int argc, char** argv, fd_t* dupfd) {
    for (size_t i = 0; i < 3; i++) {
        dupfd[i] = EOF;
    }
    int outappend = 0;
    int errappend = 0;

    char* infile = NULL;
    char* outfile = NULL;
    char* errfile = NULL;

    for (size_t i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "<") && (i + 1) < argc) {
            infile = argv[i + 1];
            argv[i] = NULL;
            i++;
            continue;
        }
        // todo "<<"
        if (!strcmp(argv[i], ">") && (i + 1) < argc) {
            outfile = argv[i + 1];
            argv[i] = NULL;
            outappend = O_TRUNC;
            i++;
            continue;
        }
        if (!strcmp(argv[i], ">>") && (i + 1) < argc) {
            outfile = argv[i + 1];
            argv[i] = NULL;
            outappend = O_APPEND;
            i++;
            continue;
        }
        if (!strcmp(argv[i], "2>") && (i + 1) < argc) {
            errfile = argv[i + 1];
            argv[i] = NULL;
            i++;
            continue;
        }
        if (!strcmp(argv[i], "2>>") && (i + 1) < argc) {
            errfile = argv[i + 1];
            argv[i] = NULL;
            errappend = O_APPEND;
            i++;
            continue;
        }
    }

    if (infile != NULL) {
        fd_t fd = open(infile, O_RDONLY | O_CREAT, 0755);
        if (fd == EOF) {
            printf("open infile %s failure\n", infile);
            goto clean;
        }
        dupfd[0] = fd;
    }
    if (outfile != NULL) {
        fd_t fd = open(outfile, O_WRONLY | outappend | O_CREAT, 0755);
        if (fd == EOF) {
            printf("open outfile %s failure\n", outfile);
            goto clean;
        }
        dupfd[1] = fd;
    }
    if (errfile != NULL) {
        fd_t fd = open(outfile, O_WRONLY | errappend | O_CREAT, 0755);
        if (fd == EOF) {
            printf("open errfile %s failure\n", outfile);
            exit(EOF);
        }
        dupfd[2] = fd;
    }
    return;
clean:
    for (size_t i = 0; i < 3; i++) {
        if (dupfd[i] != EOF)
            close(dupfd[i]);
    }
}

pid_t builtin_command(char* filename,
                      char* argv[],
                      fd_t infd,
                      fd_t outfd,
                      fd_t errfd) {
    int status;

    pid_t pid = fork();
    if (pid) {
        // 父进程文件无用，直接关闭
        if (infd != EOF) {
            close(infd);
        }
        if (outfd != EOF) {
            close(outfd);
        }
        if (errfd != EOF) {
            close(errfd);
        }
        return pid;
    }
    if (infd != EOF) {
        fd_t fd = dup2(infd, STDIN_FILENO);
        close(infd);
    }
    if (outfd != EOF) {
        fd_t fd = dup2(outfd, STDOUT_FILENO);
        close(outfd);
    }
    if (errfd != EOF) {
        fd_t fd = dup2(outfd, STDERR_FILENO);
        close(errfd);
    }

    int i = execve(filename, argv, envp);
    exit(i);
}

static void builtin_exec(int argc, char* argv[]) {
    stat_t statbuf;
    sprintf(buf, "/bin/%s.out", argv[0]);
    if (stat(buf, &statbuf) == EOF) {
        printf("lsh: commnand not fount: %s\n", argv[0]);
        return;
    }

    int status;
    fd_t dupfd[3];
    dupfile(argc, argv, dupfd);

    pid_t pid = builtin_command(buf, &argv[1], dupfd[0], dupfd[1], dupfd[2]);
    waitpid(pid, &status, 0);
}

static void execute(int argc, char* argv[]) {
    char* line = argv[0];
    if (!strcmp(line, "test")) {
        return builtin_test(argc, argv);
    } else if (!strcmp(line, "logo")) {
        return builtin_logo();
    } else if (!strcmp(line, "pwd")) {
        return builtin_pwd();
    } else if (!strcmp(line, "clear")) {
        return builtin_clear();
    } else if (!strcmp(line, "exit")) {
        return builtin_exit(argc, argv);
        // } else if (!strcmp(line, "ls")) {
        //     return builtin_ls(argc, argv);
    } else if (!strcmp(line, "cd")) {
        return builtin_cd(argc, argv);
        // } else if (!strcmp(line, "cat")) {
        //     return builtin_cat(argc, argv);
    } else if (!strcmp(line, "mkdir")) {
        return builtin_mkdir(argc, argv);
    } else if (!strcmp(line, "rmdir")) {
        return builtin_rmdir(argc, argv);
    } else if (!strcmp(line, "rm")) {
        return builtin_rm(argc, argv);
    } else if (!strcmp(line, "date")) {
        return builtin_date();
    } else if (!strcmp(line, "mount")) {
        return builtin_mount(argc, argv);
    } else if (!strcmp(line, "umount")) {
        return builtin_umount(argc, argv);
    } else if (!strcmp(line, "mkfs")) {
        return builtin_mkfs(argc, argv);
    }
    return builtin_exec(argc, argv);
}

int lsh_main(void) {
    memset(cmd, 0, sizeof(cmd));
    memset(cwd, 0, sizeof(cwd));

    builtin_logo();

    while (true) {
        print_prompt();
        readline(cmd, sizeof(cmd));
        if (cmd[0] == 0) {
            continue;
        }
        int argc = cmd_parse(cmd, argv, ' ');
        if (argc < 0 || argc >= MAX_ARG_NR) {
            continue;
        }
        execute(argc, argv);
    }
    return 0;
}