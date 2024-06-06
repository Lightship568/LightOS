#include <lib/debug.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <lightos/memory.h>
#include <sys/assert.h>
#include <sys/types.h>

#define ZONE_VALID 1     // ards 可用区域
#define ZONE_RESERVED 2  // ards 不可用区域

#define IDX(addr) ((u32)addr >> 12)  // 获取 ards 页索引
#define PAGE(idx) ((u32)idx << 12)   // 获取 idx 对应的页所在地址
#define ASSERT_PAGE(addr) assert((addr & 0xfff) == 0)
#define USED 100  // mem_map 已使用，学 linux 放一个大值

// BIOS 内存检测结果结构体
typedef struct ards_t {
    u64 base;  // 内存基地址
    u64 size;  // 内存长度
    u32 type;  // 类型
} _packed ards_t;

static u32 memory_base = 0;  // 可用内存区域基地址，当前可用 0x10000 也就是 1M
static u32 memory_size = 0;  // 可用内存大小
static u32 total_pages = 0;  // 所有内存页面数
static u32 free_pages = 0;   // 空闲内存页数

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
            DEBUGK("Memory base 0x%p size 0x%x type %d\n", (u32)ptr->base,
                   (u32)ptr->size, (u32)ptr->type);
            if (ptr->type == ZONE_VALID && ptr->size > memory_size) {
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

    if (memory_size + memory_base < KERNEL_MEM_SIZE){
        panic("Memory too small, at least %dM needed for kernel!\n", KERNEL_MEM_SIZE/MEMORY_BASE);
    }

    assert(memory_base == MEMORY_BASE);
    assert((memory_size & 0xfff) == 0);

    total_pages = IDX(memory_size) + IDX(MEMORY_BASE);
    free_pages = IDX(memory_size);

    DEBUGK("Total pages %d\n", total_pages);
    DEBUGK("Free pages %d\n", free_pages);
}

static u32 start_page_idx = 0;  // 可分配物理内存起始地址页索引
static u8* mem_map;             // 大名鼎鼎的 memory_map!
static u32 mem_map_pages;       // 物理内存数组占用的页数

void memory_map_init(void) {
    mem_map = (u8*)memory_base;

    mem_map_pages = div_round_up(total_pages, PAGE_SIZE);
    free_pages -= mem_map_pages;
    DEBUGK("Memory map page count %d\n", mem_map_pages);

    // 清空物理内存数组
    memset((void*)mem_map_pages, 0, mem_map_pages * PAGE_SIZE);

    // "占用" 前 1M 的内存位置 以及 物理内存数组已经占用的页
    start_page_idx = IDX(MEMORY_BASE) + mem_map_pages;
    for (size_t i = 0; i < start_page_idx; ++i) {
        mem_map[i] = USED;
    }
    DEBUGK("Total pages %d free pages %d\n", total_pages, free_pages);
}

// 获取一页物理内存
static u32 get_page() {
    for (size_t i = start_page_idx; i < total_pages; ++i) {
        if (!mem_map[i]) {  // 该物理页未被占用
            mem_map[i] = 1;
            free_pages--;
            assert(free_pages >= 0);
            u32 page_addr = PAGE(i);
            DEBUGK("Get page 0x%p\n", page_addr);
            return page_addr;
        }
    }
    panic("Out of Memory!");
}

// 释放一页物理内存
static void put_page(u32 addr) {
    ASSERT_PAGE(addr);

    u32 idx = IDX(addr);
    // 确保 idx 大于受保护的内存 IDX（1M + mem_map）
    assert(idx >= start_page_idx && idx < total_pages);
    // 确保页原本有引用计数
    assert(mem_map[idx] >= 1);

    mem_map[idx] -= 1;
    if (!mem_map[idx]) {  // 释放+1
        free_pages += 1;
    }

    // 确保空闲页数量在正确范围内
    assert(free_pages > 0 && free_pages < total_pages);

    DEBUGK("Put page 0x%p\n", addr);
}

// 将 cr0 寄存器最高位 PG 置为 1，启用分页
static _inline void enable_page() {
    // 0b1000_0000_0000_0000_0000_0000_0000_0000
    // 0x80000000
    asm volatile(
        "movl %cr0, %eax\n"
        "orl $0x80000000, %eax\n"
        "movl %eax, %cr0\n");
}

// 设置 cr3 寄存器，参数是页目录的地址
void set_cr3(u32 pde) {
    ASSERT_PAGE(pde);
    asm volatile("movl %%eax, %%cr3\n" ::"a"(pde));
}

// 初始化页表项
static void entry_init(page_entry_t* entry, u32 index) {
    *(u32*)entry = 0;

    entry->present = 1;
    entry->write = 1;
    entry->user = 1;
    entry->index = index;
}

// 初始化内存映射
void mapping_init(void) {
    page_entry_t* pde = (page_entry_t*)KERNEL_PAGE_DIR;
    // 清空前 4 个 pde
    memset(pde, 0, PAGE_SIZE * KERNEL_PAGE_DIR_COUNT);

    u32 index = 0;
    // 设置 pde 前 total_page_table_pages 项目指向顺应页表地址
    for (size_t i = 0; i < KERNEL_PAGE_TABLE_COUNT; ++i) {
        page_entry_t* pte = (page_entry_t*)(KERNEL_PAGE_TABLE + PAGE_SIZE * i);
        memset(pte, 0, PAGE_SIZE);
        entry_init(&pde[i], IDX(pte));
        for (size_t tidx = 0; tidx < (PAGE_SIZE / 4); ++tidx, ++index) {
            //跳过对0x0的映射，方便后面的空指针触发 PF 来排错。
            if (index == 0){
                continue;
            }
            entry_init(&pte[tidx], index); // IDX(tidx * PAGE_SIZE) == tidx
            mem_map[index] = USED;
        }
    }

    // 设置 cr3 寄存器
    set_cr3((u32)pde);

    // 分页有效
    enable_page();

    DEBUGK("Mapping initilized\n");
}