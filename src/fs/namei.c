#include <lib/debug.h>
#include <lib/string.h>
#include <lightos/fs.h>
#include <lightos/stat.h>
#include <sys/assert.h>
#include <sys/types.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

// 判断文件名是否相等
bool match_name(const char* name, const char* entry_name, char** next) {
    char* lhs = (char*)name;
    char* rhs = (char*)entry_name;

    while (*lhs == *rhs && *lhs != EOS && *rhs != EOS) {
        lhs++;
        rhs++;
    }
    if (*rhs) {
        return false;
    }
    if (*lhs) {
        if (!IS_SEPARATOR(*lhs)) {
            return false;
        }
        lhs++;
    }
    *next = lhs;
    return true;
}

// 获取 dir 目录下的 name 目录所在的 dentry_t 和 cache_t
cache_t* find_entry(inode_t** dir,
                           const char* name,
                           char** next,
                           dentry_t** result) {
    assert(ISDIR((*dir)->desc->mode));

    // 获得目标目录的子目录数量
    u32 entries = (*dir)->desc->size / sizeof(dentry_t);

    idx_t i = 0;
    idx_t block = 0;
    cache_t* pcache = NULL;
    dentry_t* entry = NULL;

    for (; i < entries; i++, entry++) {
        // 首次进入，或当前 entry 已经遍历了一整个块需要切换到下一个数据块
        if (!pcache || (u32)entry >= (u32)pcache->data + BLOCK_SIZE) {
            brelse(pcache); // 释放之前的缓存
            block = bmap((*dir), i / BLOCK_DENTRIES, false);
            assert(block);

            pcache = bread((*dir)->dev, block);
            entry = (dentry_t*)pcache->data;
        }
        if (match_name(name, entry->name, next)) {
            *result = entry;
            return pcache;
        }
    }
    brelse(pcache);
    return NULL;
}

extern time_t sys_time(void);

// 在 dir 目录中添加 name 目录项
cache_t* add_entry(inode_t* dir, const char* name, dentry_t** result) {
    char* next = NULL;
    
    cache_t* pcache = find_entry(&dir, name, &next, result);
    // 若已经存在，则直接返回
    if (pcache) {
        return pcache;
    }

    // name 中不能有分隔符
    for (size_t i = 0; i < NAME_LEN && name[i]; ++i) {
        assert(!IS_SEPARATOR(name[i]));
    }

    idx_t i = 0;
    idx_t block = 0;
    dentry_t* entry = NULL;

    // 可能存在之前删除的 dentry 将 nr 置 0, 因此创建优先扫描一遍。
    for (; true; i++, entry++) {
        if (!pcache || (u32)entry >= (u32)pcache->data + BLOCK_SIZE) {
            brelse(pcache);
            block = bmap(dir, i / BLOCK_DENTRIES, true); // 创建，为可能的添加做准备
            assert(block);

            pcache = bread(dir->dev, block);
            entry = (dentry_t*)pcache->data;
        }
        // 已扫描一遍 nr 都非 0，增加新的 dentry
        if (i * sizeof(dentry_t) >= dir->desc->size) {
            entry->nr = 0;
            dir->desc->size = (i + 1) * sizeof(dentry_t);
            // dir->cache->dirty = true;
        }
        if (entry->nr){
            continue;
        }

        strncpy(entry->name, name, NAME_LEN);
        pcache->dirty = true;
        dir->desc->mtime = sys_time();
        dir->cache->dirty = true;
        *result = entry;
        return pcache;
    }
}

void dir_test(){
    task_t* task = get_current();
    inode_t* inode = task->iroot;
    inode->count++; // 对应下面的 iput
    char* next = NULL;
    dentry_t* entry = NULL;
    cache_t* pcache = NULL;


    dev_t dev = inode->dev;   

    pcache = find_entry(&inode, "hello.txt", &next, &entry);
    idx_t nr = entry->nr;
    brelse(pcache);

    pcache = add_entry(inode, "world.txt", &entry);
    entry->nr = nr;

    inode_t* hello = iget(dev, nr);
    hello->desc->nlinks++;
    hello->cache->dirty = true;
    iput(inode);
    iput(hello);
    brelse(pcache);

    // char* pathname = "d1/d2/d3/d4";
    // char* name = pathname;
    // pcache = find_entry(&inode, name, &next, &entry);
    // brelse(pcache);
    // iput(inode);
    
    // inode = iget(dev, entry->nr);
    // name = next;
    // pcache = find_entry(&inode, name, &next, &entry);
    // brelse(pcache);
    // iput(inode);

    // inode = iget(dev, entry->nr);
    // name = next;
    // pcache = find_entry(&inode, name, &next, &entry);
    // brelse(pcache);
    // iput(inode);

    // inode = iget(dev, entry->nr);
    // name = next;
    // pcache = find_entry(&inode, name, &next, &entry);
    // brelse(pcache);
    // iput(inode);

}