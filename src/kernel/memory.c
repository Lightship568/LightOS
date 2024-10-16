#include <lib/bitmap.h>
#include <lib/debug.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <lightos/memory.h>
#include <lightos/multiboot2.h>
#include <sys/assert.h>
#include <sys/types.h>

#define ZONE_VALID 1     // ards 可用区域
#define ZONE_RESERVED 2  // ards 不可用区域

#define IDX(addr) ((u32)addr >> 12)  // 获取 ards 页索引
#define PAGE(idx) ((u32)idx << 12)   // 获取 idx 对应的页所在地址
#define ASSERT_PAGE(addr) assert((addr & 0xfff) == 0)
#define USED 100  // mem_map 已使用，学 linux 放一个大值

bitmap_t kernel_map;

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

    u32 count = 0;
    // bootloader 启动
    ards_t* ptr;  // loader 启动的 address range descriptor structure

    // grub 启动
    multi_tag_t* tag;
    u32 size_mbi;
    multi_tag_mmap_t* mtag;
    multi_mmap_entry_t* entry;

    DEBUGK("memory_init...\n");

    // 引导来源
    if (magic == LIGHTOS_MAGIC) {
        // boot + loader 启动
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

    } else if (magic == MULTIBOOT2_MAGIC) {
        // grub 启动
        size_mbi = *(unsigned int*)addr;
        tag = (multi_tag_t*)(addr + 8);
        DEBUGK("Announced multiboot infomation size 0x%x\n", size_mbi);
        while (tag->type != MULTIBOOT_TAG_TYPE_END) {
            if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
                break;
            }
            // 下一个 tag 对齐到了 8 字节
            tag = (multi_tag_t*)((u32)tag + ((tag->size + 7) & ~7));
        }
        mtag = (multi_tag_mmap_t*)tag;
        entry = mtag->entries;
        while ((u32)entry < (u32)tag + tag->size) {
            DEBUGK("Memory base 0x%p, size 0x%p, type %d\n", (u32)entry->addr, (u32)entry->len, (u32)entry->type);
            count++;
            if (entry->type == ZONE_VALID && entry->len > memory_size){
                memory_base = (u32)entry->addr;
                memory_size = (u32)entry->len;
            }
            entry = (multi_mmap_entry_t *)((u32)entry + mtag->entry_size);
        }

    } else {
        panic("Memory init magic unknown 0x%p\n", magic);
    }

    DEBUGK("ARDS count %d\n", count);
    DEBUGK("Memory base 0x%p\n", (u32)memory_base);
    DEBUGK("Memory size 0x%p\n", (u32)memory_size);

    if (memory_size + memory_base < KERNEL_MEM_SIZE) {
        panic("Memory too small, at least %dM needed for kernel!\n",
              KERNEL_MEM_SIZE / MEMORY_BASE);
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

// mmap 物理内存数组初始化，顺便占用（1M+mmap占用）的物理地址
void memory_map_init(void) {
    mem_map = (u8*)memory_base + KERNEL_PAGE_DIR_VADDR; // 增加虚拟地址偏移。

    mem_map_pages = div_round_up(total_pages, PAGE_SIZE);
    free_pages -= mem_map_pages;
    DEBUGK("Memory map page count %d\n", mem_map_pages);

    // 清空物理内存数组
    memset((void*)mem_map, 0, mem_map_pages * PAGE_SIZE);

    // "占用" 前 1M 的内存位置 以及 物理内存数组已经占用的页
    start_page_idx = IDX(MEMORY_BASE) + mem_map_pages;
    for (size_t i = 0; i < start_page_idx; ++i) {
        mem_map[i] = USED;
    }
    DEBUGK("Total pages %d free pages %d\n", total_pages, free_pages);

    // 初始化内核虚拟内存位图，需要 8 bits 对齐
    // 这里需要跳过mmap，但是为了管理方便，选择了将mmap的bitmap置1了
    u32 length = (IDX(KERNEL_MEM_SIZE) - IDX(MEMORY_BASE)) / 8;
    // 增加内核高地址偏移
    bitmap_init(&kernel_map, (u8*)KERNEL_MAP_BITS_VADDR, length, IDX(MEMORY_BASE)+IDX(KERNEL_PAGE_DIR_VADDR));
    bitmap_scan(&kernel_map, mem_map_pages);
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

// 初始化内存映射，也就是完成最初的页表映射，实现内核物理地址=线性地址
void mapping_init(void) {
    /**
     * 注意，这里只初始化了一个PD和两个PT，一个PT可以映射1024*4K也就是4M的内存。
     * 所以当前映射了 8M 给内核。
     * 
     * PD永远都是1个，这是根据分页的地址分割决定的，10/10/12的分割刚好让一个页的PD可以覆盖10位的索引。
     * 64位也一样，四级分页9/9/9/9/12（48bits），加载到cr3的PD永远只有一个页，不存在跨页的情况。
     * 
     * 后续为了给内核分配更多空间（1G），需要增加PDE（PT）到 1G/4M = 256 个项。
     */
    page_entry_t* pde = (page_entry_t*)KERNEL_PAGE_DIR_PADDR;
    // 清空 pde
    memset(pde, 0, PAGE_SIZE);

    u32 index = 0;
    // 设置 pde 前 total_page_table_pages 项目指向顺应页表地址
    for (size_t i = 0, i2 = (KERNEL_PAGE_DIR_VADDR >> 22); i < KERNEL_PAGE_TABLE_COUNT; ++i, ++i2) {
        page_entry_t* pte = (page_entry_t*)(KERNEL_PAGE_TABLE + PAGE_SIZE * i);
        memset(pte, 0, PAGE_SIZE);
        entry_init(&pde[i], IDX(pte));
        entry_init(&pde[i2], IDX(pte));
        for (size_t tidx = 0; tidx < (PAGE_SIZE / 4); ++tidx, ++index) {
            // 跳过对0x0的映射，方便后面的空指针触发 PF 来排错。
            // if (index == 0) {
            //     continue;
            // }
            // 高地址线性地址不需要跳过，后面会清理内核位于低地址的的pde
            entry_init(&pte[tidx], index);  // IDX(tidx * PAGE_SIZE) == tidx
        }
    }


    // 设置 cr3 寄存器
    set_cr3((u32)pde);

    // 分页有效
    enable_page();

}

// 将启动时设置的低地址页表映射清空
void unset_low_mapping(void) {
    memset((void *)KERNEL_PAGE_DIR_VADDR, 0, sizeof(page_entry_t) * KERNEL_PAGE_TABLE_COUNT);
}

// 从bit_map中分配 count 个连续的页
// todo 设置mmap
static u32 _alloc_page(bitmap_t* map, u32 count) {
    assert(count > 0);

    int32 index = bitmap_scan(map, count);

    if (index == EOF) {
        panic("alloc page scan failed");
    }

    u32 addr = PAGE(index);

    // DEBUGK("Scan page 0x%p count %d\n", addr, count);
    return addr;
}

// 与 _alloc_page 相对，重置相应的页
static void _reset_page(bitmap_t* map, u32 addr, u32 count) {
    ASSERT_PAGE(addr);
    assert(count > 0);
    u32 index = addr >> 12;

    for (size_t i = 0; i < count; i++) {
        assert(bitmap_test(map, index + i));
        bitmap_set(map, index + i, 0);
    }
}

// 分配 count 个连续的内核页
u32 alloc_kpage(u32 count) {
    assert(count > 0);
    u32 vaddr = _alloc_page(&kernel_map, count);
    DEBUGK("Alloc kernel pages 0x%p count %d\n", vaddr, count);
    return vaddr;
}

// 释放 count 个连续的内核页
void free_kpage(u32 vaddr, u32 count) {
    ASSERT_PAGE(vaddr);
    assert(count > 0);
    _reset_page(&kernel_map, vaddr, count);
    DEBUGK("Free kernel pages 0x%p count %d\n", vaddr, count);
}

void link_page(u32 vaddr){

}

void unlink_page(u32 vaddr){

}