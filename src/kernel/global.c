#include <lib/debug.h>
#include <lib/string.h>
#include <sys/global.h>

descriptor_t gdt[GDT_SIZE];
pointer_t gdt_ptr;

void gdt_init() {
    asm volatile("sgdt gdt_ptr");

    memcpy(&gdt, (void*)gdt_ptr.base, gdt_ptr.limit + 1);

    gdt_ptr.base = (u32)&gdt;
    gdt_ptr.limit = sizeof(gdt) - 1;

    // 下面这行不加\n编译会报二进制格式错误，估计是汇编和c混在了一起
    // 也可以用内联汇编语法确保没有歧义
    // asm volatile("lgdt gdt_ptr\n"); 
    asm volatile ("lgdt %0" : : "m" (gdt_ptr));

    DEBUGK("GDT Initialized at 0x%p\n", gdt);
}