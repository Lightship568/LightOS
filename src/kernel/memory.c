#include <lightos/memory.h>
#include <lib/debug.h>
#include <lightos/memory.h>
#include <sys/assert.h>

#define ZONE_VALID 1     // ards 可用区域
#define ZONE_RESERVED 2  // ards 不可用区域

#define IDX(addr) ((u32)addr >> 12)  // 获取 ards 页索引

// BIOS 内存检测结果结构体
typedef struct ards_t {
    u64 base;  // 内存基地址
    u64 size;  // 内存长度
    u32 type;  // 类型
} _packed ards_t;

u32 memory_base = 0;  // 可用内存区域基地址，当前可用 0x10000 也就是 1M
u32 memory_size = 0;  // 可用内存大小
u32 total_pages = 0;  // 所有内存页面
u32 free_pages = 0;   // 空闲内存页数

#define used_pages (total_pages - free_pages)  // 已用页数

void memory_init(u32 magic, u32 addr) {
    DEBUGK("memory_init...\n");
    u32 count;
    ards_t* ptr;

    // loader 进入内核会有 magic
    if (magic == LIGHTOS_MAGIC) {
        count = *(u32*)addr;
        ptr = (ards_t*)(addr + 4);

        for (size_t i = 0; i < count; ++i, ++ptr) {
            DEBUGK("Memory base 0x%p size 0x%p type %d\n", (u32)ptr->base, (u32)ptr->size, (u32)ptr->type);
            if (ptr->type == ZONE_VALID && ptr->size > memory_size){
                memory_base = (u32)ptr->base;
                memory_size = (u32)ptr->size; 
            }
        }

    } else {
        panic("Memory init magic unknown 0x%p\n", magic);
    }

    DEBUGK("ARDS count %d\n", count);
    DEBUGK("Memory base 0x%p\n", (u32)memory_base);
    DEBUGK("Memory size 0x%p\n", (u32)memory_size);


    assert(memory_base == MEMORY_BASE);
    assert((memory_size & 0xfff) == 0);

    total_pages = IDX(memory_size) + IDX(MEMORY_BASE);
    free_pages = IDX(memory_size);

    DEBUGK("Total pages %d\n", total_pages);
    DEBUGK("Free pages %d\n", free_pages);
}   
