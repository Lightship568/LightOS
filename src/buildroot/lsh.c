#include <lib/stdlib.h>
#include <lib/string.h>
#include <lib/syscall.h>
#include <lightos/fs.h>
#include <sys/assert.h>

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
    test();
}

void readline(char* buf, u32 count) {
    assert(buf != NULL);
    char* ptr = buf;
    u32 idx = 0;
    char ch = 0;
    while (idx < count) {
        ptr = buf + idx;
        int ret = read(stdin, ptr, 1);
        if (ret == -1) {
            *ptr = 0;
            return;
        }
        switch (*ptr) {
            case '\n':
            case '\r':
                *ptr = 0;
                ch = '\n';
                write(stdout, &ch, 1);
                return;
            case '\b':
                if (idx > 0) {
                    idx--;
                    ch = '\b';
                    write(stdout, &ch, 1);
                }
                break;
            case '\t':
                continue;
            default:
                write(stdout, ptr, 1);
                idx++;
                break;
        }
    }
    buf[idx] = '\0';
}

static int cmd_parse(char* cmd, char* argv[], char token) {
    assert(cmd != NULL);

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

static void builtin_ls(int argc, char* argv[]) {
    fd_t fd;
    int i = 1;
    if (argc < 2) {
        fd = open(cwd, O_RDONLY, 0);
        printf("ls dir of %s:\n", cwd);
        goto ls;
    }
    for (; i < argc; ++i) {
        fd = open(argv[i], O_RDONLY, 0);
        printf("ls dir of %s:\n", argv[i]);
    ls:
        int size = readdir(fd, buf, BUFLEN);
        if (size < 0){
            printf("ls: error reading dir\n");
            continue;
        }
        for (int j = 0; j < (size / sizeof(dirent_t)); ++j) {
            dirent_t* dir = &((dirent_t*)buf)[j];
            if (dir->nr != 0){
                printf("%s\n", dir->name);
            }
        }
    }
}

static void builtin_cd(int argc, char* argv[]) {
    if (argc > 1) {
        if(chdir(argv[1]) < 0){
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
        if (fd <= 2){
            printf("cat: no such file %s\n", argv[i]);
            continue;
        }
        while(true){
            int len = read(fd, buf, BUFLEN);
            if (len == EOF){
                break;
            }
            write(stdout, buf, len);
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
        if (mkdir(argv[i], 0755) < 0){
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
        if (rmdir(argv[i]) < 0){
            printf("rmdir: error removing dir %s\n", argv[i]);
        }else{
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
    } else if (!strcmp(line, "ls")) {
        return builtin_ls(argc, argv);
    } else if (!strcmp(line, "cd")) {
        return builtin_cd(argc, argv);
    } else if (!strcmp(line, "cat")) {
        return builtin_cat(argc, argv);
    } else if (!strcmp(line, "mkdir")) {
        return builtin_mkdir(argc, argv);
    } else if (!strcmp(line, "rmdir")) {
        return builtin_rmdir(argc, argv);
    } else if (!strcmp(line, "rm")) {
        return builtin_rm(argc, argv);
    }
    printf("lsh: commnand not fount: %s\n", argv[0]);
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