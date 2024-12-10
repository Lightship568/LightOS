#include <lib/bitmap.h>
#include <lib/debug.h>
#include <lib/elf.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <lightos/fs.h>
#include <lightos/memory.h>
#include <lightos/task.h>
#include <lib/arena.h>
#include <sys/assert.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

// 检查 ELF 文件头
static bool elf_validate(Elf32_Ehdr* ehdr) {
    // 不是 ELF 文件
    if (memcmp(&ehdr->e_ident, "\177ELF\1\1\1", 7))
        return false;

    // 不是可执行文件
    if (ehdr->e_type != ET_EXEC)
        return false;

    // 不是 386 程序
    if (ehdr->e_machine != EM_386)
        return false;

    // 版本不可识别
    if (ehdr->e_version != EV_CURRENT)
        return false;

    if (ehdr->e_phentsize != sizeof(Elf32_Phdr))
        return false;

    return true;
}

static void load_segment(inode_t* inode, Elf32_Phdr* phdr) {
    assert(phdr->p_align == 0x1000);       // 对齐到页
    assert((phdr->p_vaddr & 0xfff) == 0);  // 对齐到页

    u32 vaddr = phdr->p_vaddr;

    // 需要页的数量
    u32 count = div_round_up(MAX(phdr->p_memsz, phdr->p_filesz), PAGE_SIZE);

    // 目前没有懒加载，提前 link 所有用到的页。
    for (size_t i = 0; i < count; i++) {
        u32 addr = vaddr + i * PAGE_SIZE;
        assert(addr >= USER_EXEC_ADDR && addr < USER_MMAP_ADDR);
        link_user_page(addr);
    }
    // 直接读进去
    inode_read(inode, (char*)vaddr, phdr->p_filesz, phdr->p_offset);

    // 如果文件大小大于内存大小，则需要将 .bss 置零
    if (phdr->p_filesz < phdr->p_memsz) {
        memset((char*)vaddr + phdr->p_filesz, 0,
               phdr->p_memsz - phdr->p_filesz);
    }

    // 如果段不可写，则置为只读
    if ((phdr->p_flags & PF_W) == 0) {
        for (size_t i = 0; i < count; i++) {
            u32 addr = vaddr + i * PAGE_SIZE;
            page_entry_t* entry = get_pte(addr);
            entry->write = false;
            entry->readonly = true;
            flush_tlb(addr);
            kunmap(GET_PAGE(entry));  // 清理 get_pte 的 kmap
        }
    }

    task_t* task = get_current();
    if (phdr->p_flags == (PF_R | PF_X)) {  // 代码段
        task->text = vaddr;
    } else if (phdr->p_flags == (PF_R | PF_W)) {  // 数据段
        task->data = vaddr;
    }

    // 设置 .end
    task->end = vaddr + count * PAGE_SIZE;
}

static u32 load_elf(inode_t* inode) {
    link_user_page(USER_EXEC_ADDR);

    int n = 0;
    // 读取 ELF 文件头
    n = inode_read(inode, (char*)USER_EXEC_ADDR, sizeof(Elf32_Ehdr), 0);
    assert(n == sizeof(Elf32_Ehdr));

    Elf32_Ehdr* ehdr = (Elf32_Ehdr*)USER_EXEC_ADDR;
    if (!elf_validate(ehdr)) {
        return EOF;
    }

    // 读取程序段头
    Elf32_Phdr* phdr = (Elf32_Phdr*)(USER_EXEC_ADDR + sizeof(Elf32_Ehdr));
    n = inode_read(inode, (char*)phdr, ehdr->e_phnum * ehdr->e_phentsize,
                   ehdr->e_phoff);

    Elf32_Phdr* ptr = phdr;
    for (size_t i = 0; i < ehdr->e_phnum; ++i) {
        if (ptr->p_type != PT_LOAD) {
            continue;
        }
        load_segment(inode, ptr);
        ptr++;
    }
    return ehdr->e_entry;
}


// 清理 fork 的残留，准备加载新 elf (filename 将被截断)
void fork_clean(task_t* task, char* filepath){
    /**
     * 释放掉原本程序的空间布局和资源
     * 与 sys_exit 相似，但是要新申请和清理部分资源
     */

    // todo 清空 vmap 和 mmap

    // iroot 默认都是根目录不变
    // 修改 pwd
    char* ptr = strrsep(filepath);
    *(++ptr) = '\0';
    sys_chdir(filepath);

    // 释放 fork 的二进制 inode
    iput(task->iexec);

    // 释放所有打开文件，标准流不释放
    for (size_t i = 3; i < TASK_FILE_NR; ++i) {
        file_t* file = task->files[i];
        if (file) {
            sys_close(i);
        }
    }

    // 最后释放所有user_page, PTs，否则 filepath 访存失败
    // 但不可释放内核部分，因此 pd 不释放
    free_pte(task);
    // 但是需要刷新tlb，这里直接拷贝新页表后 set_cr3 可能高效一些
    set_cr3(task->pde);
}

int sys_execve(char* filename, char* argv[], char* envp[]) {
    inode_t* inode = namei(filename);
    int ret = EOF;
    if (!inode) {
        goto clean;
    }
    // 不是常规文件
    if (!ISFILE(inode->desc->mode)) {
        goto clean;
    }
    // 文件不可执行
    if (!permission(inode, P_EXEC)) {
        goto clean;
    }

    task_t* task = get_current();
    strncpy(task->name, filename, TASK_NAME_LEN);

    // 清理 fork 后的残留资源
    // 注意，从此开始用户态页全部释放，需提前将参数复制到内核
    fork_clean(task, filename);



    // todo 环境变量与参数

    u32 entry = load_elf(inode);
    if (entry == EOF) {
        goto clean;
    }

    // 设置堆内存地址（没必要调用 sys_brk）
    task->brk = task->end;
    // 设置 iexec
    task->iexec = inode;

    // ROP
    intr_frame_t* iframe =
        (intr_frame_t*)((u32)task + PAGE_SIZE - sizeof(intr_frame_t));

    iframe->vector = 0x20;
    iframe->edi = 1;
    iframe->esi = 2;
    iframe->ebp = USER_STACK_TOP - 1;
    iframe->esp_dummy = 4;
    iframe->ebx = 5;
    iframe->edx = 6;
    iframe->ecx = 7;
    iframe->eax = 8;

    iframe->gs = 0;
    iframe->ds = USER_DATA_SELECTOR;
    iframe->es = USER_DATA_SELECTOR;
    iframe->fs = USER_DATA_SELECTOR;
    iframe->ss = USER_DATA_SELECTOR;  // 当前的GDT配置下，用户可以修改内核内存。

    iframe->cs = USER_CODE_SELECTOR;

    iframe->error = LIGHTOS_MAGIC;

    iframe->eip = entry;
    iframe->eflags = (0 << 12 | 0b10 | 1 << 9);  // IOPL=0, 固定1, IF中断开启
    iframe->esp = USER_STACK_TOP;
    

    asm volatile(
        "movl %0, %%esp\n"
        "jmp interrupt_exit\n" ::"m"(iframe));

clean:
    iput(inode);
    return ret;
}
