#include <lib/stdio.h>
#include <sys/types.h>
#include <lib/syscall.h>

int main(void) {
    if (getpid() != 1){
        printf("cannot run multiple init\n");
        return 0;
    }

    while (true) {
        u32 status;
        pid_t pid = fork();
        if (pid) {
            pid_t child = waitpid(pid, &status, -1);
            printf("[init] wait pid %d status %d at time %d\n", child,
                   status, time());
        } else {
            int err = execve("/bin/lsh.out", NULL, NULL);
            printf("[init] execve /bin/lsh.out error with return %d\n", err);
            printf("[init] exit\n");
            exit(err); // init 退出后，OS 将会 panic
        }
    }
}
