#include <lib/syscall.h>
#include <lib/signal.h>
#include <lib/stdlib.h>

int main(int argc, char* argv[]){
    if (argc < 2){
        return -1;
    }
    return kill(atoi(argv[1]), SIGTERM);
}