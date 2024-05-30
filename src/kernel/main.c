/**
 * main.c
*/

#include <lightos/lightos.h>
#include <lightos/io.h>
#include <lightos/console.h>
#include <sys/types.h>
#include <lib/stdarg.h>
#include <lib/print.h>

char message[] = "hello LightOS!!!111111111111111111111111111111111111111111111\n\0";


void kernel_init(){
    console_init(); 
    for (int i = 0; i < 50; ++i){
        printk("%-*d%s",5,i,message);
    }
    while(true);
    return;
} 