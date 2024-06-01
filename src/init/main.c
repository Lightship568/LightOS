/**
 * main.c
*/

#include <lightos/lightos.h>
#include <lightos/console.h>
#include <sys/types.h>
#include <lib/print.h>
#include <lib/assert.h>
#include <lib/debug.h>
#include <sys/global.h>
#include <sys/interrupt.h>

char message[] = "hello \nLightOS!!!111111111111111111111111111111111111111111111\n\0";


void kernel_init(void){
    console_init(); 
    for (int i = 0; i < 50; ++i){
        printk("%-*d%s",5,i,message);
    }
    gdt_init();
    interrupt_init();
    
    asm volatile (
    "int $0x80" 
    : 
    : "a"(0)
    : "memory"
    );

    while(true);
    return;
} 