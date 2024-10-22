#include <lib/bitmap.h>
#include <lib/debug.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <lightos/memory.h>
#include <lightos/multiboot2.h>
#include <sys/assert.h>
#include <sys/types.h>
#include <lightos/task.h>

#define ZONE_VALID 1     // ards 可用区域
#define ZONE_RESERVED 2  // ards 不可用区域


#define GET_PAGE(addr) ((u32)addr & 0xFFFFF000) //获取地址所在页的地址
#define IDX(addr) ((u32)addr >> 12)  // 获取 ards 页索引
#define PAGE(idx) ((u32)idx << 12)   // 获取 idx 对应的页所在地址
#define ASSERT_PAGE(addr) assert((addr & 0xfff) == 0)
#define USED 100  // mem_map 已使用，学 linux 放一个大值

#define LOW_MEM_PADDR_TO_VADDR(paddr) ((u32)paddr + KERNEL_PAGE_DIR_VADDR) // 获取物理地址对应的内核虚拟地址（低地址直接映射部分）
#define LOW_MEM_VADDR_TO_PADDR(vaddr) ((u32)vaddr - KERNEL_PAGE_DIR_VADDR) // 获取内核虚拟地址对应的物理地址（低地址直接映射部分）

#define PDE_IDX(addr) ((u32)addr >> 22) // 获取pde索引（取高10位）
#define PTE_IDX(addr) (((u32)addr >> 12) & 0x3ff) // 获取pte索引（取中10位）

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

// 从bit_map中分配 count 个连续的页，并设置mmap
static u32 _alloc_page(bitmap_t* map, u32 count) {
    assert(count > 0);

    int32 index = bitmap_scan(map, count);
    if (index == EOF) {
        panic("alloc page scan failed");
    }
    u32 vaddr = PAGE(index);
    index = index - map->offset + IDX(MEMORY_BASE); // 从0开始的物理地址页的IDX
    // 设置mmap
    for (int i = 0; i < count; ++i){
        assert(!mem_map[index]);
        mem_map[index] = 1;
        index++;
        free_pages--;
        assert(free_pages >= 0);
    }
    return vaddr;
}

// 与 _alloc_page 相对，重置相应的页
static void _reset_page(bitmap_t* map, u32 vaddr, u32 count) {
    ASSERT_PAGE(vaddr);
    assert(count > 0);
    u32 index = vaddr >> 12;

    for (size_t i = 0; i < count; i++) {
        assert(bitmap_test(map, index + i));
        bitmap_set(map, index + i, 0);
    }

    // 设置mmap
    index = index - map->offset + IDX(MEMORY_BASE);
    for (int i = 0; i < count; ++i){
        assert(mem_map[index]);
        mem_map[index]--;
        if (mem_map[index] == 0){
            free_pages++;
        }
        index++;
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

// 获取一页用户内存（phy 8M+），返回物理地址
static u32 get_user_page() {
    for (size_t i = IDX(KERNEL_MEM_SIZE); i < total_pages; ++i) {
        if (!mem_map[i]) {  // 该物理页未被占用
            mem_map[i] = 1;
            free_pages--;
            assert(free_pages >= 0);
            u32 paddr = PAGE(i);
            DEBUGK("Get free page 0x%p\n", paddr);
            return paddr;
        }
    }
    panic("get_user_page: Out of Memory!\nTotal memory: 0x%x bytes\n", total_pages * PAGE_SIZE);
}

// 释放一页用户内存
static void put_user_page(u32 paddr) {
    ASSERT_PAGE(paddr);

    u32 idx = IDX(paddr);
    // 确保 idx 大于受保护的内存 IDX（1M + mem_map）
    assert(idx >= IDX(KERNEL_MEM_SIZE) && idx < total_pages);
    // 确保页原本有引用计数
    assert(mem_map[idx] >= 1);

    mem_map[idx] -= 1;
    if (!mem_map[idx]) {  // 释放+1
        free_pages += 1;
    }

    // 确保空闲页数量在正确范围内
    assert(free_pages > 0 && free_pages < total_pages);

    DEBUGK("Put user page paddr 0x%p\n", paddr);
}
/********************************************************
 * 页表操作
 * 
 ********************************************************/

// 刷新tlb快表
static _inline void flush_tlb(u32 vaddr){
    asm volatile("invlpg (%0)\n" ::"r"(vaddr):"memory");
}

static page_entry_t* get_pde(){
    //todo 后续需要从 current 的 pde 中取
    return (page_entry_t*)KERNEL_PAGE_DIR_VADDR;
}

// kmap需要维护一个p-v的映射关系 kmap_poll，方便重入检查和unmap操作
typedef struct kmap_mapping{
    u32 present : 1;    // 存在标记
    u32 cnt : 31;       // 映射次数
    u32 paddr;          // 物理地址
}kmap_mapping; //8字节，必须被PAGE_SIZE整除！

// mapping_list刚好存放一个pte对应的所有mapping，即1024个
#define KMAP_POOL_LEN (PAGE_SIZE / sizeof(u32))

static kmap_mapping kmap_pool[KMAP_POOL_LEN]; // 用1024*8（2页）来存储
static page_entry_t* kmap_pte;
static u32 kmap_start_vaddr; //kmap映射的起始虚拟地址，当前是3G+8M

#define KMAP_GET_VADDR(index) ((u32)(kmap_start_vaddr + PAGE(index)))
#define KMAP_GET_INDEX(vaddr) (PTE_IDX((u32)vaddr - kmap_start_vaddr))

void kmap_init(void){
    kmap_start_vaddr = LOW_MEM_PADDR_TO_VADDR(KERNEL_MEM_SIZE);
    // 8M+3G偏移，是当前的pde[0x300 + 2]的位置，即当前内核pde的顺延位。
    page_entry_t* pde_entry = &(get_pde())[PDE_IDX(kmap_start_vaddr)];
    // 分配一个页的pte专门用来映射kmap
    kmap_pte = (page_entry_t*)alloc_kpage(1);
    memset(kmap_pte, 0, PAGE_SIZE);
    // 设置页表增加kmap映射（此时pte尚未设置具体内容）
    entry_init(pde_entry, IDX(LOW_MEM_VADDR_TO_PADDR(kmap_pte)));
    // 同时清空kmap_pool
    memset(kmap_pool, 0, sizeof(kmap_mapping) * KMAP_POOL_LEN);
    DEBUGK("Kmap initialized\n");
}

// 映射8M以上高物理内存进内核虚拟地址访问
u32 kmap(u32 paddr){
    assert(paddr >= KERNEL_MEM_SIZE);
    ASSERT_PAGE(paddr);    
    u32 vaddr;

    // 查找kmap_pool中是否已经映射
    int i = 0;
    for (; i < KMAP_POOL_LEN; ++i){
        if (kmap_pool[i].present == 0){
            break;
        }
        if (kmap_pool[i].paddr == paddr){
            kmap_pool[i].cnt++;
            return KMAP_GET_VADDR(i);
        }
    }
    if (i == KMAP_POOL_LEN){
        panic("No enough pte for kmap!\n");
    }
    // 没有映射，映射到PAGE(i)的虚拟地址上
    kmap_pool[i].present = 1;
    kmap_pool[i].cnt = 1;
    kmap_pool[i].paddr = paddr;
    vaddr = KMAP_GET_VADDR(i);

    entry_init(kmap_pte, IDX(paddr));
    flush_tlb(vaddr);

    DEBUGK("kmap paddr 0x%x at vaddr 0x%x\n", paddr, vaddr);
    return vaddr;
}

u32 kunmap(u32 vaddr){
    DEBUGK("kunmap vaddr 0x%x\n", vaddr);
    // 保证是kmap分配的虚拟地址范围
    assert(vaddr >= kmap_start_vaddr && vaddr < kmap_start_vaddr + KMAP_POOL_LEN * PAGE_SIZE);
    ASSERT_PAGE(vaddr);
    
    u32 idx = KMAP_GET_INDEX(vaddr);

    assert(kmap_pool[idx].present);
    kmap_pool[idx].present = 0;
    kmap_pool[idx].cnt--;
    if (kmap_pool[idx].cnt == 0){
        kmap_pte[idx].present = 0;
        flush_tlb(vaddr);
    }
}


// 用户态获取或创建进程pte entry，返回entry虚拟地址，注意调用方需要清理kmap
static page_entry_t* get_pte(u32 vaddr){
    page_entry_t* pde = get_pde();
    u32 idx = PDE_IDX(vaddr);
    page_entry_t* entry = &pde[idx];
    u32 paddr_page;
    u32 kvaddr;

    // 创建pde对应的pt
    if (!entry->present){
        paddr_page = get_user_page();
        entry_init(entry, IDX(paddr_page)); //pde[idx]=paddr_page
        kvaddr = kmap(paddr_page); // 注意调用方清理kmap
        memset((void*)kvaddr, 0, PAGE_SIZE);
        DEBUGK("Create new page table for 0x%x at pde[0x%x]\n", vaddr, idx);
    } else {
        paddr_page = PAGE(entry->index);
        kvaddr = kmap(paddr_page); // 注意调用方清理kmap
    }
    
    idx = PTE_IDX(vaddr);
    entry = &((page_entry_t*)kvaddr)[idx];
    return entry;
}

void link_user_page(u32 vaddr){
    // 找到该虚拟地址的页表pte
    task_t *task = get_current();
    page_entry_t* entry;
    u32 paddr_page;
    u32 idx = IDX(vaddr);
    bitmap_t* vmap = task->vmap;

    entry = get_pte(vaddr);

    // 已经映射，无需操作
    if (entry->present){
        assert(bitmap_test(vmap, idx));
        return;
    }

    // 设置vmap位图
    assert(!bitmap_test(vmap, idx));
    bitmap_set(vmap, idx, true);

    // 为进程页表增加新的物理页
    paddr_page = get_user_page();
    entry_init(entry, IDX(paddr_page));
    flush_tlb(vaddr);

    DEBUGK("Link new user page for 0x%x at 0x%x\n", vaddr, paddr_page);

    kunmap(GET_PAGE(entry)); //清理get_pte的kmap

}

void unlink_user_page(u32 vaddr){

    ASSERT_PAGE(vaddr); //传入虚拟地址所在页地址

    // 找到该虚拟地址的页表pte
    task_t *task = get_current();
    page_entry_t* entry;
    u32 idx = IDX(vaddr);
    bitmap_t* vmap = task->vmap;

    entry = get_pte(vaddr);

    if (!entry->present){
        assert(!bitmap_test(vmap, idx));
        return;
    }

    assert(bitmap_test(vmap, idx));
    bitmap_set(vmap, idx, false);

    entry->present = 0;
    flush_tlb(vaddr);

    put_user_page(PAGE(entry->index));
    
    kunmap(GET_PAGE(entry)); //清理get_pte的kmap

    DEBUGK("Unlink user page for 0x%x at paddr 0x%x\n", vaddr, PAGE(entry->index));
}