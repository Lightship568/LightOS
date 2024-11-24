/**
 * main.c
 */

#include <lib/arena.h>
#include <lib/debug.h>
#include <lib/print.h>
#include <lib/stdlib.h>
#include <lightos/cache.h>
#include <lightos/console.h>
#include <lightos/ide.h>
#include <lightos/interrupt.h>
#include <lightos/lightos.h>
#include <lightos/memory.h>
#include <lightos/rtc.h>
#include <lightos/task.h>
#include <lightos/time.h>
#include <sys/assert.h>
#include <sys/global.h>
#include <sys/types.h>
#include <lightos/fs.h>

extern void clock_init(void);    // clock.c 无 .h
extern void syscall_init(void);  // gate.c
extern void schedule(void);
extern void tss_init(void);
extern void idle(void);

void kernel_init(void) {
    tss_init();  // 初始化 GDT[3] TSS
    memory_map_init();

    interrupt_init();
    time_init();
    clock_init();
    rtc_init();

    syscall_init();
    task_init();

    aerna_init();
    kmap_init();

    ide_init();         // 硬盘驱动初始化
    page_cache_init();  // 缓冲初始化
    file_init();

    inode_init(); // 文件系统 inode 初始化
    super_init(); // 文件系统超级块初始化

    idle();  // set stack & sti
}