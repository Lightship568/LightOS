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
#include <lib/stdlib.h>
#include <lightos/task.h>

char message[] = "hello LightOS!!!111111111111111111111111111111111111111111111\n\0";


void kernel_init(void){
    console_init(); 
    for (int i = 0; i < 50; ++i){
        printk("%-*d%s",5,i,message);
    }
    gdt_init();
    interrupt_init();
    task_test_init();
    start_interrupt();

    // int a = 0;
    // printk("%d",10/a);
    
    asm volatile ("int $0x80\n");

    u32 counter = 0;
    while(true){
        // DEBUGK("looping in kernel init %d...\n", counter++);
        delay(100000000);
    }
    return;
} 