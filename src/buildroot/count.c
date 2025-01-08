#include <lib/syscall.h>
#include <lib/stdio.h>

int main(int argc, char const *argv[]){
    int counter = 1;
    while (counter){
        printf("counter %d\a\n", counter++);
        sleep(1000);
    }
    return 0;
}