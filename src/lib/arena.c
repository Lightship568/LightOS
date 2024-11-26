#include <lib/arena.h>
#include <lightos/memory.h>
#include <sys/assert.h>
#include <lib/string.h>
#include <lib/debug.h>
#include <lib/print.h>
#include <lib/mutex.h>
#include <lib/string.h>

/**
 * 内核用 kmalloc 与 kfree
 */

static list_t free_list;
static list_t used_list;
static mutex_t arena_lock;

#define ARENA_MAGIC 0xA1B2C3    // 30bits
#define MIN_BLOCK_SIZE 16       // 16 字节最小堆块

// 分配新的页作为 free arena 
static void _arena_add_new_pages(size_t size){
    int page_cnt = (size + sizeof(arena_t)) / PAGE_SIZE + 1;
    arena_t* parena;

    parena = (arena_t*)alloc_kpage(page_cnt);
    parena->magic = ARENA_MAGIC;
    parena->is_head = true;
    parena->back_ptr = NULL;
    parena->forward_ptr = NULL;
    parena->is_used = 0;
    parena->buf = (void*)((u32)parena + sizeof(arena_t));
    parena->length = PAGE_SIZE * page_cnt - sizeof(arena_t);
    list_push(&free_list, &parena->list);
}

void* kmalloc(size_t size) {
    arena_t* parena = NULL;
    arena_t* parena_divided = NULL;
    void* buf;
    list_node_t* p_node_find;

    if (size == 0){
        return NULL;
    }

    mutex_lock(&arena_lock);

    size = size >= MIN_BLOCK_SIZE? size: MIN_BLOCK_SIZE;

    if (list_empty(&free_list)) {
        _arena_add_new_pages(size);
    }

refind:
    p_node_find = free_list.head.next;
    while (p_node_find != &free_list.tail){
        parena = element_entry(arena_t, list, p_node_find);
        if (parena->length >= sizeof(arena_t) + size){
            break;
        }
        parena = NULL;
        p_node_find = p_node_find->next;
    }
    
    if (p_node_find == &free_list.tail){ // 如果没有够大的arena，就分配一下，refind
        _arena_add_new_pages(size);
        goto refind;
    }

    // parena 已经是待拆分的大块 arena
    assert(parena != NULL);
    list_remove(p_node_find);
    list_push(&used_list, p_node_find);
    if (parena->length - size < sizeof(arena_t) + MIN_BLOCK_SIZE) { // 剩下的空间不足一个新的块，不拆分
        parena->is_used = true;
    } else{ // 拆分
        parena->is_used = true;
        parena_divided = (arena_t*)((u32)parena->buf + size);
        parena_divided->back_ptr = parena;
        parena_divided->forward_ptr = parena->forward_ptr;
        parena_divided->buf = (void*)((u32)parena_divided + sizeof(arena_t));
        parena_divided->length = parena->length - size - sizeof(arena_t);
        parena_divided->is_used = false;
        parena_divided->is_head = false;
        parena_divided->magic = ARENA_MAGIC;
        parena->forward_ptr = parena_divided;
        parena->length = size;
        list_push(&free_list, &parena_divided->list);
    }

    memset(parena->buf, 0, size);

    mutex_unlock(&arena_lock);

    return parena->buf;
}

void kfree(void *ptr) {
    if (!ptr) return;

    // 传入的ptr是buf实际位置，不是buf指针位置（偏移0x4，buf没啥用）
    arena_t* parena = (arena_t*)(ptr - sizeof(arena_t));

    // overflow magic check
    if (parena->forward_ptr && parena->forward_ptr->magic != ARENA_MAGIC){
        printk("parena: 0x%p\n", parena);
        printk("length: 0x%p\n", parena);
        printk("next ptr: 0x%p\n", parena->forward_ptr);
        printk("next ptr magic: 0x%x != Magic(0x%x)\n", parena->forward_ptr->magic, ARENA_MAGIC);
        panic("overwrite the next arena!\n");
    }
    
    mutex_lock(&arena_lock);
    assert(list_search(&used_list, &parena->list));

    list_remove(&parena->list); // 从used_list中删除
    list_push(&free_list, &parena->list); //加入free_list
    parena->is_used = false;

    // 向上合并，找到最开始的free块
    while (parena->back_ptr && !parena->back_ptr->is_used){
        parena = parena->back_ptr;
    }

    // 向下合并
    while (parena->forward_ptr && !parena->forward_ptr->is_used){
        list_remove(&parena->forward_ptr->list); // 将下一块从free_list删除
        parena->length += parena->forward_ptr->length + sizeof(arena_t);
        parena->forward_ptr = parena->forward_ptr->forward_ptr;
    }

    mutex_unlock(&arena_lock);
}

void aerna_init(void){
    list_init(&free_list);
    list_init(&used_list);
    mutex_init(&arena_lock);
    DEBUGK("Aerna initialized\n");
}