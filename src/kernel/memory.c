#include <lib/bitmap.h>
#include <lib/debug.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <lightos/fs.h>
#include <lightos/memory.h>
#include <lightos/multiboot2.h>
#include <sys/assert.h>
#include <sys/types.h>

// 关闭 memory 的内核注释
#define LOGK(fmt, args...) DEBUGK(fmt, ##args)
// #define LOGK(fmt, args...) ;
#define LOGK_KMAP(fmt, args...) ;        // kmap 操作
#define LOGK_ALLOC_OPTS(fmt, args...) ;  // 申请与释放相关操作
#define LOGK_PT_OPTS(fmt, args...) DEBUGK(fmt, ##args)     // 页表相关操作

#define ZONE_VALID 1     // ards 可用区域
#define ZONE_RESERVED 2  // ards 不可用区域

// #define GET_PAGE(addr) ((u32)addr & 0xFFFFF000)  // 获取地址所在页的地址
#define IDX(addr) ((u32)addr >> 12)  // 获取 ards 页索引
#define PAGE(idx) ((u32)idx << 12)   // 获取 idx 对应的页所在地址
#define ASSERT_PAGE(addr) assert((addr & 0xfff) == 0)
#define USED 100  // mem_map 已使用，学 linux 放一个大值

#define LOW_MEM_PADDR_TO_VADDR(paddr) \
    ((u32)paddr +                     \
     KERNEL_PAGE_DIR_VADDR)  // 获取物理地址对应的内核虚拟地址（低地址直接映射部分）
#define LOW_MEM_VADDR_TO_PADDR(vaddr) \
    ((u32)vaddr -                     \
     KERNEL_PAGE_DIR_VADDR)  // 获取内核虚拟地址对应的物理地址（低地址直接映射部分）

#define PDE_IDX(addr) ((u32)addr >> 22)  // 获取pde索引（取高10位）
#define PTE_IDX(addr) (((u32)addr >> 12) & 0x3ff)  // 获取pte索引（取中10位）

bitmap_t kernel_map;

/******************************************************************************************************************
 * 内存管理初始化
 *
 *****************************************************************************************************************/

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

    LOGK("memory_init...\n");

    // 引导来源
    if (magic == LIGHTOS_MAGIC) {
        // boot + loader 启动
        count = *(u32*)addr;
        ptr = (ards_t*)(addr + 4);

        for (size_t i = 0; i < count; ++i, ++ptr) {
            LOGK("Memory base 0x%p size 0x%x type %d\n", (u32)ptr->base,
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
        LOGK("Announced multiboot infomation size 0x%x\n", size_mbi);
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
            LOGK("Memory base 0x%p, size 0x%p, type %d\n", (u32)entry->addr,
                 (u32)entry->len, (u32)entry->type);
            count++;
            if (entry->type == ZONE_VALID && entry->len > memory_size) {
                memory_base = (u32)entry->addr;
                memory_size = (u32)entry->len;
            }
            entry = (multi_mmap_entry_t*)((u32)entry + mtag->entry_size);
        }

    } else {
        panic("Memory init magic unknown 0x%p\n", magic);
    }

    LOGK("ARDS count %d\n", count);
    LOGK("Memory base 0x%p\n", (u32)memory_base);
    LOGK("Memory size 0x%p\n", (u32)memory_size);

    if (memory_size + memory_base < KERNEL_MEM_SIZE) {
        panic("Memory too small, at least %dM needed for kernel!\n",
              KERNEL_MEM_SIZE / MEMORY_BASE);
    }

    assert(memory_base == MEMORY_BASE);
    assert((memory_size & 0xfff) == 0);

    total_pages = IDX(memory_size) + IDX(MEMORY_BASE);
    free_pages = IDX(memory_size);

    LOGK("Total pages %d\n", total_pages);
    LOGK("Free pages %d\n", free_pages);
}

static u32 start_page_idx = 0;  // 可分配物理内存起始地址页索引
static u8* mem_map;             // 大名鼎鼎的 memory_map!
static u32 mem_map_pages;       // 物理内存数组占用的页数

// mmap 物理内存数组初始化，顺便占用（1M+mmap占用）的物理地址
void memory_map_init(void) {
    mem_map = (u8*)memory_base + KERNEL_VADDR_OFFSET;  // 增加虚拟地址偏移。

    mem_map_pages = div_round_up(total_pages, PAGE_SIZE);
    free_pages -= mem_map_pages;
    LOGK("Memory map page count %d\n", mem_map_pages);

    // 清空物理内存数组
    memset((void*)mem_map, 0, mem_map_pages * PAGE_SIZE);

    // "占用" 前 1M 的内存位置 以及 物理内存数组已经占用的页
    start_page_idx = IDX(MEMORY_BASE) + mem_map_pages;
    for (size_t i = 0; i < start_page_idx; ++i) {
        mem_map[i] = USED;
    }
    LOGK("Total pages %d free pages %d\n", total_pages, free_pages);

    // 初始化内核虚拟内存位图，需要 8 bits 对齐
    // 这里需要跳过mmap，但是为了管理方便，选择了将mmap的bitmap置1了
    u32 length = (IDX(KERNEL_MEM_SIZE) - IDX(MEMORY_BASE)) / 8;
    // 增加内核高地址偏移
    bitmap_init(&kernel_map, (u8*)KERNEL_MAP_BITS_VADDR, length,
                IDX(MEMORY_BASE) + IDX(KERNEL_PAGE_DIR_VADDR));
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
    for (size_t i = 0, i2 = (KERNEL_PAGE_DIR_VADDR >> 22);
         i < KERNEL_PAGE_TABLE_COUNT; ++i, ++i2) {
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
    memset((void*)KERNEL_PAGE_DIR_VADDR, 0,
           sizeof(page_entry_t) * KERNEL_PAGE_TABLE_COUNT);
}

/******************************************************************************************************************
 * 内核工具函数
 *
 * ****************************************************************************************************************/

// 设置 cr3 寄存器，参数是页目录的地址
void set_cr3(u32 pde) {
    ASSERT_PAGE(pde);
    asm volatile("movl %%eax, %%cr3\n" ::"a"(pde));
}

// 从bit_map中分配 count 个连续的页，并设置mmap
static u32 _alloc_kpage(bitmap_t* map, u32 count) {
    assert(count > 0);

    int32 index = bitmap_scan(map, count);
    if (index == EOF) {
        panic("alloc page scan failed");
    }
    u32 vaddr = PAGE(index);
    index = index - map->offset + IDX(MEMORY_BASE);  // 从0开始的物理地址页的IDX
    // 设置mmap
    for (int i = 0; i < count; ++i) {
        assert(!mem_map[index]);
        mem_map[index] = 1;
        index++;
        free_pages--;
        assert(free_pages >= 0);
    }
    return vaddr;
}

// 与 _alloc_kpage 相对，重置相应的页
static void _reset_kpage(bitmap_t* map, u32 vaddr, u32 count) {
    ASSERT_PAGE(vaddr);
    assert(count > 0);
    u32 index = vaddr >> 12;
    u32 paddr_index;

    // 设置mmap
    paddr_index = index - map->offset + IDX(MEMORY_BASE);
    for (int i = 0; i < count; ++i) {
        assert(mem_map[paddr_index]);
        mem_map[paddr_index]--;
        if (mem_map[paddr_index] == 0) {
            free_pages++;
            // 内核vmap，只有在物理内存释放后才能重置vmap
            assert(bitmap_test(map, index + i));
            bitmap_set(map, index + i, 0);
        }
        paddr_index++;
    }
}

// 分配 count 个连续的内核页
u32 alloc_kpage(u32 count) {
    assert(count > 0);
    u32 vaddr = _alloc_kpage(&kernel_map, count);
    LOGK_ALLOC_OPTS("Alloc kernel pages 0x%p count %d\n", vaddr, count);
    return vaddr;
}

// 释放 count 个连续的内核页
void free_kpage(u32 vaddr, u32 count) {
    ASSERT_PAGE(vaddr);
    assert(count > 0);
    _reset_kpage(&kernel_map, vaddr, count);
    LOGK_ALLOC_OPTS("Free kernel pages 0x%p count %d\n", vaddr, count);
}

// 获取一页用户内存（phy 16M+），返回物理地址
static u32 get_user_page() {
    for (size_t i = IDX(KERNEL_MEM_SIZE); i < total_pages; ++i) {
        if (!mem_map[i]) {  // 该物理页未被占用
            mem_map[i] = 1;
            free_pages--;
            assert(free_pages >= 0);
            u32 paddr = PAGE(i);
            LOGK_ALLOC_OPTS("Get free page 0x%p\n", paddr);
            return paddr;
        }
    }
    panic("get_user_page: Out of Memory!\nTotal memory: 0x%x bytes\n",
          total_pages * PAGE_SIZE);
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

    LOGK_ALLOC_OPTS("Put user page paddr 0x%p\n", paddr);
}
/****************************************************************************************************************
 * 页表操作
 *
 ****************************************************************************************************************/

void flush_tlb(u32 vaddr) {
    asm volatile("invlpg (%0)\n" ::"r"(vaddr) : "memory");
}

static page_entry_t* get_pde() {
    // 需要从 current 的 pde 中取虚拟地址
    return (page_entry_t*)(get_current()->pde + KERNEL_VADDR_OFFSET);
}

void copy_pde(task_t* target_task) {
    page_entry_t* pde = (page_entry_t*)alloc_kpage(1);
    // 拷贝页表
    memcpy((void*)pde, (void*)(get_current()->pde + KERNEL_VADDR_OFFSET),
           PAGE_SIZE);
    target_task->pde = (u32)pde - KERNEL_VADDR_OFFSET;
}

void copy_pte(task_t* target_task) {
    page_entry_t* current_pde_entry = get_pde();
    page_entry_t* target_pde_entry;
    page_entry_t* current_pte;
    page_entry_t* target_pte;
    u32 target_pte_paddr;
    page_entry_t* temp_pte_entry;  // 遍历pte使用

    copy_pde(target_task);
    target_pde_entry = (page_entry_t*)(target_task->pde + KERNEL_VADDR_OFFSET);

    size_t i = 0;
    // 用户页表PT拷贝（phy > 16M）
    for (; i < PDE_IDX(KERNEL_VADDR_OFFSET);
         ++i, current_pde_entry++, target_pde_entry++) {  // 0-0x300
        if (!current_pde_entry->present)
            continue;
        target_pte_paddr = get_user_page();
        // PDE需要更新到新PT
        entry_init(target_pde_entry, IDX(target_pte_paddr));
        // >16M kmap 进内核
        current_pte = (page_entry_t*)kmap(PAGE(current_pde_entry->index));
        target_pte = (page_entry_t*)kmap(target_pte_paddr);
        // 设置 CoW
        temp_pte_entry = current_pte;
        for (size_t j = 0; j < (PAGE_SIZE / sizeof(page_entry_t));
             ++j, temp_pte_entry++) {
            if (!temp_pte_entry->present)
                continue;
            // 若不是共享内存，则设置只读
            if (!temp_pte_entry->shared) {
                // 设置不可写
                temp_pte_entry->write = false;
            }
            // 增加引用计数
            mem_map[temp_pte_entry->index]++;
            assert(mem_map[temp_pte_entry->index] < 255);
        }
        // 拷贝PT内容（包括新设置的write位）
        memcpy(target_pte, current_pte, PAGE_SIZE);
        // 取消两个pte的映射
        kunmap((u32)current_pte);
        kunmap((u32)target_pte);
    }

    // 修改了只读，需要刷新快表，否则父进程仍可以根据tlb从而写入共享页
    set_cr3(get_current()->pde);

    // 因为内核完全是共享的，不应该直接加解引用这么玩

    // 内核页已经由copy_pde拷贝设置共享
    // 只需要设置 >=1M 的 mem_map 引用情况
    // for (; i < PDE_IDX(KERNEL_VADDR_OFFSET) + KERNEL_PAGE_TABLE_COUNT; ++i,
    // current_pde_entry++){
    //     if (!current_pde_entry->present) continue;
    //     temp_pte_entry = (page_entry_t*)(PAGE(current_pde_entry->index) +
    //     KERNEL_VADDR_OFFSET); for (size_t j = 0; j <
    //     (PAGE_SIZE/sizeof(page_entry_t)); ++j, temp_pte_entry++){
    //         if (i == 0x300 && j < (0x100 + mem_map_pages)) continue;
    //         //4k*256==1M，需要跳过 <1M 与 mem_map 范围 if
    //         (mem_map[temp_pte_entry->index] == 0) continue; //
    //         内核页表已经全部映射，只需要对已有 mem_map 增引用即可
    //         mem_map[temp_pte_entry->index]++;
    //     }
    // }
}

void free_pte(task_t* target_task) {
    page_entry_t* pde_entry;
    page_entry_t* pte_base;
    page_entry_t* pte_entry;
    u32 pte_paddr;
    pde_entry = get_pde();

    LOGK_PT_OPTS("Free all PTEs in user space\n");

    size_t i = 0;
    // 用户页表释放（phy > 16M）
    for (; i < PDE_IDX(KERNEL_VADDR_OFFSET); ++i, pde_entry++) {  // 0-0x300
        if (!pde_entry->present)
            continue;
        pte_paddr = PAGE(pde_entry->index);
        pte_base = (page_entry_t*)kmap(pte_paddr);
        pte_entry = pte_base;
        for (size_t j = 0; j < (PAGE_SIZE / sizeof(page_entry_t));
             ++j, pte_entry++) {
            if (!pte_entry->present)
                continue;
            // 释放用户内存
            put_user_page(PAGE(pte_entry->index));
            pte_entry->present = false;
            // 理论上即将被释放或 set_cr3，因此可以不刷新 tlb
            // flush_tlb(PAGE(pte_entry->index));
        }
        kunmap((u32)pte_base);
        // 释放 PT 所在页
        put_user_page(pte_paddr);
        // 置 !present
        pde_entry->present = false;
    }

    // 因为内核完全是共享的，不应该直接加解引用这么玩
    // 比如下面这个pd或者vmap之类的就是创建新进程kalloc出来的，在这里释放的话就会与主动释放发生冲突，逻辑也不好
    // 没法判断是否是本进程的页还是其他的进程的页，所以干脆不修改引用了。

    // 内核页减少 mem_map 引用计数
    // for (; i < PDE_IDX(KERNEL_VADDR_OFFSET) + KERNEL_PAGE_TABLE_COUNT; ++i,
    // pde_entry++){
    //     if (!pde_entry->present) continue;
    //     pte_entry = (page_entry_t*)(PAGE(pde_entry->index) +
    //     KERNEL_VADDR_OFFSET); for (size_t j = 0; j <
    //     (PAGE_SIZE/sizeof(page_entry_t)); ++j, pte_entry++){
    //         if (i == 0x300 && j < (0x100 + mem_map_pages)) continue;
    //         //4k*256==1M，需要跳过 <1M 与 mem_map 范围 if
    //         (mem_map[pte_entry->index] == 0) continue; //
    //         内核页表已经全部映射，只需要对已有 mem_map 解引用即可
    //         free_kpage(PAGE(pte_entry->index) + KERNEL_VADDR_OFFSET, 1); //
    //         减少引用计数，归零则释放bitmap
    //     }
    // }
}

// kmap需要维护一个p-v的映射关系 kmap_poll，方便重入检查和unmap操作
typedef struct kmap_mapping {
    u32 present : 1;  // 存在标记
    u32 cnt : 31;     // 映射次数
    u32 paddr;        // 物理地址
} kmap_mapping;       // 8字节，必须被PAGE_SIZE整除！

// mapping_list刚好存放一个pte对应的所有mapping，即1024个
#define KMAP_POOL_LEN (PAGE_SIZE / sizeof(u32))

static kmap_mapping kmap_pool[KMAP_POOL_LEN];  // 用1024*8（2页）来存储
static page_entry_t* kmap_pte;
static u32 kmap_start_vaddr;  // kmap映射的起始虚拟地址，当前是3G+16M

#define KMAP_GET_VADDR(index) ((u32)(kmap_start_vaddr + PAGE(index)))
#define KMAP_GET_INDEX(vaddr) (PTE_IDX((u32)vaddr - kmap_start_vaddr))

void kmap_init(void) {
    kmap_start_vaddr = LOW_MEM_PADDR_TO_VADDR(KERNEL_MEM_SIZE);
    // 16M+3G偏移，是当前的pde[0x300 + 4]的位置，即紧邻当前内核的虚拟空间。
    // 物理上内核16M后紧接着用户空间，虚拟上内核后面是kmap的用户态空间。
    page_entry_t* pde_entry = &(get_pde())[PDE_IDX(kmap_start_vaddr)];
    // 分配一个页的pte专门用来映射kmap
    kmap_pte = (page_entry_t*)alloc_kpage(1);
    memset(kmap_pte, 0, PAGE_SIZE);
    // 设置页表增加kmap映射（此时pte尚未设置具体内容）
    entry_init(pde_entry, IDX(LOW_MEM_VADDR_TO_PADDR(kmap_pte)));
    // 同时清空kmap_pool
    memset(kmap_pool, 0, sizeof(kmap_mapping) * KMAP_POOL_LEN);
    LOGK("Kmap initialized\n");
}

// 映射16M以上高物理内存进内核虚拟地址访问
u32 kmap(u32 paddr) {
    assert(paddr >= KERNEL_MEM_SIZE);
    ASSERT_PAGE(paddr);
    u32 vaddr;

    // 查找kmap_pool中是否已经映射
    int i = 0;
    for (; i < KMAP_POOL_LEN; ++i) {
        if (kmap_pool[i].present == 0) {
            break;
        }
        if (kmap_pool[i].paddr == paddr) {
            kmap_pool[i].cnt++;
            return KMAP_GET_VADDR(i);
        }
    }
    if (i == KMAP_POOL_LEN) {
        panic("No enough pte for kmap!\n");
    }
    // 没有映射，映射到PAGE(i)的虚拟地址上
    kmap_pool[i].present = 1;
    kmap_pool[i].cnt = 1;
    kmap_pool[i].paddr = paddr;
    vaddr = KMAP_GET_VADDR(i);

    entry_init(&kmap_pte[i], IDX(paddr));
    flush_tlb(vaddr);

    LOGK_KMAP("kmap paddr 0x%x at vaddr 0x%x\n", paddr, vaddr);
    return vaddr;
}

u32 kunmap(u32 vaddr) {
    LOGK_KMAP("kunmap vaddr 0x%x\n", vaddr);
    // 保证是kmap分配的虚拟地址范围
    assert(vaddr >= kmap_start_vaddr &&
           vaddr < kmap_start_vaddr + KMAP_POOL_LEN * PAGE_SIZE);
    ASSERT_PAGE(vaddr);

    u32 idx = KMAP_GET_INDEX(vaddr);

    // free_pte 过程会置零 present，因此不能这样判断
    // assert(kmap_pool[idx].present)

    kmap_pool[idx].present = 0;
    kmap_pool[idx].cnt--;
    if (kmap_pool[idx].cnt == 0) {
        kmap_pte[idx].present = 0;
        flush_tlb(vaddr);
    }
}

page_entry_t* get_pte(u32 vaddr) {
    page_entry_t* pde = get_pde();
    u32 idx = PDE_IDX(vaddr);
    page_entry_t* entry = &pde[idx];
    u32 paddr_page;
    u32 kvaddr;

    // 创建pde对应的pt
    if (!entry->present) {
        paddr_page = get_user_page();
        entry_init(entry, IDX(paddr_page));  // pde[idx]=paddr_page
        kvaddr = kmap(paddr_page);           // 注意调用方清理kmap
        memset((void*)kvaddr, 0, PAGE_SIZE);
        LOGK_PT_OPTS("Create new page table for 0x%x at pde[0x%x]\n", vaddr, idx);
    } else {
        paddr_page = PAGE(entry->index);
        kvaddr = kmap(paddr_page);  // 注意调用方清理kmap
    }

    idx = PTE_IDX(vaddr);
    entry = &((page_entry_t*)kvaddr)[idx];
    return entry;
}

void link_user_page(u32 vaddr) {
    LOGK_PT_OPTS("Linking user page for vaddr 0x%x\n", vaddr);
    // 找到该虚拟地址的页表pte
    task_t* task = get_current();
    page_entry_t* entry;
    u32 paddr_page;
    u32 idx = IDX(vaddr);

    entry = get_pte(vaddr);

    // 已经映射，无需操作
    if (entry->present) {
        LOGK_PT_OPTS("entry->present == true, skip linking\n");
        goto clean;
    }

    // 为进程页表增加新的物理页
    paddr_page = get_user_page();
    entry_init(entry, IDX(paddr_page));
    flush_tlb(vaddr);

    LOGK_PT_OPTS("Link new user page for 0x%x at 0x%x\n", vaddr, paddr_page);
clean:
    kunmap(GET_PAGE(entry));  // 清理 get_pte 的 kmap
}

void unlink_user_page(u32 vaddr) {
    ASSERT_PAGE(vaddr);  // 传入虚拟地址所在页地址

    // 找到该虚拟地址的页表pte
    task_t* task = get_current();
    page_entry_t* entry;
    u32 idx = IDX(vaddr);

    entry = get_pte(vaddr);

    if (!entry->present) {
        goto clean;
    }

    entry->present = 0;
    flush_tlb(vaddr);

    put_user_page(PAGE(entry->index));

    LOGK_PT_OPTS("Unlink user page for 0x%x at paddr 0x%x\n", vaddr,
         PAGE(entry->index));

clean:
    kunmap(GET_PAGE(entry));  // 清理 get_pte 的 kmap
}

typedef struct page_error_code_t {
    u8 present : 1;
    u8 write : 1;
    u8 user : 1;
    u8 reserved0 : 1;
    u8 fetch : 1;
    u8 protection : 1;
    u8 shadow : 1;
    u16 reserved1 : 8;
    u8 sgx : 1;
    u16 reserved2;
} _packed page_error_code_t;

void page_fault(int vector,
                u32 edi,
                u32 esi,
                u32 ebp,
                u32 esp,
                u32 ebx,
                u32 edx,
                u32 ecx,
                u32 eax,
                u32 gs,
                u32 fs,
                u32 es,
                u32 ds,
                u32 vector0,
                u32 error,
                u32 eip,
                u32 cs,
                u32 eflags,
                u32 esp3,
                u32 ss3) {
    // 读取 CR2 寄存器，获取导致缺页的虚拟地址
    u32 faulting_address;
    task_t* task;
    asm volatile("movl %%cr2, %0\n" : "=r"(faulting_address));
    LOGK("PF at vaddress 0x%x\n", faulting_address);

    task = get_current();
    // 一定是用户态才能进入 PF，如果是内核PF，理应panic
    assert(task->uid == USER_RING3);
    assert(faulting_address <= USER_STACK_TOP);

    page_error_code_t* code = (page_error_code_t*)&error;

    if (code->present) {
        // 页存在但有访问权限问题（例如，写入只读页）
        assert(code->write);

        // 发生error，应该能确认页表一定存在，所以可以使用get_pte获取已有pte
        // entry虚拟地址
        page_entry_t* pte_entry = get_pte(faulting_address);
        assert(pte_entry->present);
        assert(pte_entry->index >= IDX(MEMORY_BASE));
        assert(mem_map[pte_entry->index]);
        assert(!pte_entry->shared);  // 共享页目前不会出现权限问题。
        /**
         * 当然，这样设计对于之后的多引用计数的页来说是不合理的，
         * 但我目前想不到当前如何标记是CoW还是正常程序触发了页保护（不会在pcb加字段吧？）
         * 所以先这样，之后遇到共享页比如 IPC 或者动态链接库之后再说。
         *
         * 更新：增加 page_entry_t 的 readonly
         * 字段来标志页本来的权限，解决了上述问题
         */

        // 真正触发了不可写而非 COW
        if (pte_entry->readonly) {
            sys_exit(500);
        }

        if (mem_map[pte_entry->index] == 1) {
            // fork的另一个进程已经PF并复制过了
            pte_entry->write = true;
            LOGK("CoW: Already copy at 0x%x\n", faulting_address);
        } else {
            assert(mem_map[pte_entry->index] == 2);
            mem_map[pte_entry->index]--;
            // 为进程拷贝一个新的物理页
            u32 paddr_page = get_user_page();
            u32 page_new_vaddr = kmap(paddr_page);
            u32 page_old_vaddr = kmap(PAGE(pte_entry->index));
            memcpy((void*)page_new_vaddr, (void*)page_old_vaddr,
                   PAGE_SIZE);  // 复制内存页
            entry_init(pte_entry, IDX(paddr_page));
            flush_tlb(faulting_address);
            LOGK("CoW: Copy on Write at 0x%x\n", faulting_address);
        }

        kunmap(GET_PAGE(pte_entry));  // 清理 get_pte 的 kmap
        return;

        // 处理权限问题，或杀死进程等操作
        // DEBUGK("PF error: Access deined at 0x%x\n", faulting_address);
    } else {
        // 当前页表不存在该页，自动 link 用户栈，其余地址驳回

        // esp3 的判定好像有点问题，execve 的 free_pte 后的 pf 会传入一个很小的 esp3
        // if (esp3 <= USER_STACK_BOTTOM) {  // 用户爆栈
        //     // todo SIGSEGV 终止进程
        //     // sys_exit(-1);
        //     panic(
        //         "user stack overflow at esp: 0x%x!\n (should be 0x%x ~ 0x%x)\n",
        //         esp3, USER_STACK_BOTTOM + 1, USER_STACK_TOP);
        // }
        
        if (faulting_address > USER_STACK_BOTTOM &&
            faulting_address <= USER_STACK_TOP) {
            goto can_link;
        }
        panic("user access unmapped memory at 0x%x!\n", faulting_address);
    can_link:
        link_user_page(faulting_address);
    }

    return;
}

/******************************************************************************************************************
 * 系统调用
 *****************************************************************************************************************/

int32 sys_brk(void* addr) {
    u32 brk = (u32)addr;
    ASSERT_PAGE(brk);

    task_t* task = get_current();

    // 确保是用户进程
    assert(task->uid == USER_RING3);

    // brk 与 mmap 重叠
    if (brk >= USER_MMAP_ADDR) {
        LOGK("brk >= USER_MMAP_ADDR\n");
        return -1;
    }

    // brk 不可小于 .end
    if (brk < task->end) {
        LOGK("brk < task->end\n");
        return -1;
    }

    if (task->brk > brk) {  // 回收brk
        for (u32 page = brk; page < task->brk; page += PAGE_SIZE) {
            unlink_user_page(page);
        }
    } else if ((brk - task->brk) > PAGE(free_pages)) {  // 内存不够
        LOGK("brk out of memory\n");
        return -1;
    }
    // else 增加brk

    task->brk = brk;
    return 0;
}

void* sys_mmap(mmap_args* args) {
    void* addr = args->addr;
    size_t length = args->length;
    int prot = args->prot;
    int flags = args->flags;
    int fd = args->fd;
    off_t offset = args->offset;

    ASSERT_PAGE((u32)addr);
    assert(length > 0);

    u32 count = div_round_up(length, PAGE_SIZE);
    u32 vaddr = (u32)addr;
    task_t* task = get_current();

    if (!vaddr) {
        vaddr = bitmap_scan(task->vmap, count) * PAGE_SIZE;
    }

    assert(vaddr >= USER_MMAP_ADDR &&
           (vaddr + length) <= (USER_MMAP_ADDR + USER_MMAP_SIZE));

    // 依次为 mmap 申请的范围的页添加页表和设置权限
    for (size_t i = 0; i < count; ++i) {
        u32 page = vaddr + PAGE_SIZE * i;
        link_user_page(page);
        bitmap_set(task->vmap, IDX(page), true);

        page_entry_t* entry = get_pte(page);
        entry->user = true;
        if (prot & PROT_WRITE) {
            entry->write = true;
            entry->readonly = false;
        } else {
            entry->write = false;
            entry->readonly = true;
        }
        if (flags & MAP_SHARED) {
            entry->shared = true;
        } else if (flags & MAP_PRIVATE) {
            entry->privat = true;
        }
    }
    if (fd != EOF) {
        sys_lseek(fd, offset, SEEK_SET);
        sys_read(fd, (char*)vaddr, length);
    }
    return (void*)vaddr;
}
int sys_munmap(void* addr, size_t length) {
    task_t* task = get_current();
    u32 vaddr = (u32)addr;

    assert(vaddr >= USER_MMAP_ADDR &&
           (vaddr + length) <= (USER_MMAP_ADDR + USER_MMAP_SIZE));

    ASSERT_PAGE(vaddr);
    u32 count = div_round_up(length, PAGE_SIZE);

    for (size_t i = 0; i < count; ++i) {
        u32 page = vaddr + PAGE_SIZE * i;
        unlink_user_page(page);
        assert(bitmap_test(task->vmap, IDX(page)));
        bitmap_set(task->vmap, IDX(page), false);
    }
    return 0;
}