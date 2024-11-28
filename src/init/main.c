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

    aerna_init();

    inode_init(); // 文件系统 inode 初始化
    task_init(); // task 中增加了 idle 和 init 的 iroot & ipwd （四个引用计数），inode 初始化要提前
    kmap_init(); // kmap init 中存在 get_current, 须位于 task_init 后

    ide_init();         // 硬盘驱动初始化
    page_cache_init();  // 缓冲初始化
    file_init();

    super_init(); // 文件系统超级块初始化

    // 一切设备皆文件--文件系统设备初始化
    dev_init();

    idle();  // set stack & sti
}