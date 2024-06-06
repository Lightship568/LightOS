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
#include <lightos/time.h>
#include <lightos/rtc.h>
#include <lib/stdlib.h>
#include <lightos/memory.h>

extern void clock_init();

char message[] = "hello LightOS!!!111111111111111111111111111111111111111111111\n\0";


void kernel_init(void){
    time_init();
    gdt_init();
    interrupt_init();
    memory_map_init();
    
    clock_init();
    rtc_init();

    // memory_test();

    start_interrupt();

    mapping_init();
    char* ptr = (char *)0x400000;
    printk("0x400000: %s\n", ptr);

    ptr = (char *)(0x800000);
    printk("0x800000: %s\n", ptr);


    hang();
} 