/**
 * main.c
*/

#include <lightos/lightos.h>
#include <lightos/console.h>
#include <sys/types.h>
#include <lib/print.h>
#include <sys/assert.h>
#include <lib/debug.h>
#include <sys/global.h>
#include <lightos/interrupt.h>

extern void clock_init();

char message[] = "hello LightOS!!!111111111111111111111111111111111111111111111\n\0";


void kernel_init(void){
    console_init(); 
    gdt_init();
    interrupt_init();
    clock_init();
    start_interrupt();
    printk("\a\n");

    u32 counter = 0;
    while(true) ;
    return;
} 