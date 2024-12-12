#include <lib/stdio.h>
#include <lib/string.h>
#include <lib/syscall.h>
#include <sys/types.h>

int main(int argc, char const* argv[], char const* envp[]) {
    printf("env start!\n");
    for (size_t i = 0; i < argc; i++) {
        printf("%s\n", argv[i]);
    }
    for (size_t i = 0; 1; i++) {
        if (!envp[i])
            break;
        int len = strlen(envp[i]);
        if (len >= 1022)
            continue;
        printf("%s\n", envp[i]);
    }
    printf("env end!\n");
    return 0;
}
