/**
 * main.c
*/

#include <lightos/lightos.h>
#include <lightos/console.h>
#include <sys/types.h>
#include <lib/print.h>
#include <lib/assert.h>

char message[] = "hello \nLightOS!!!111111111111111111111111111111111111111111111\n\0";


void kernel_init(){
    console_init(); 
    for (int i = 0; i < 50; ++i){
        printk("%-*d%s",5,i,message);
    }
    assert(3>5);
    panic("%s", "wrong!!!");
    while(true);
    return;
} 