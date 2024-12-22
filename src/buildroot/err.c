#include <lib/stdio.h>
#include <lightos/memory.h>

int main(){
    char* video = (char*) 0xB8000 + KERNEL_PAGE_DIR_VADDR;
    printf("char in 0x%x is %c\n", video, *video);
    *video = 'E';
    printf("changed\n");
    return 0;
}