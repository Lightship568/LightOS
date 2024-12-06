#include <lib/elf.h>
#include <lib/debug.h>
#include <lightos/fs.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

int sys_execve(char* filename, char*argv[],char* envp[]){
    fd_t fd = sys_open(filename, O_RDONLY, 0);
    if (fd == EOF){
        return fd;
    }
    Elf32_Ehdr* ehdr = (Elf32_Ehdr*)alloc_kpage(1);
    sys_lseek(fd, 0, SEEK_SET);
    sys_read(fd,(char*)ehdr, sizeof(Elf32_Ehdr));

    sys_close(fd);

}
