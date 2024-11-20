#include <lib/debug.h>
#include <lib/string.h>
#include <lightos/fs.h>
#include <lightos/stat.h>
#include <sys/assert.h>
#include <sys/types.h>
#include <lib/string.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define P_EXEC IXOTH
#define P_WRITE IWOTH
#define P_READ IROTH

static bool permission(inode_t* inode, u16 mask) {
    u16 mode = inode->desc->mode;
    if (!inode->desc->nlinks) {
        return false;
    }
    task_t* task = get_current();
    if (task->uid == KERNEL_RING0) {
        return true;
    }
    if (task->uid == inode->desc->uid) {
        mode >>= 6;
    } else if (task->gid == inode->desc->gid) {
        mode >>= 3;
    }
    if ((mode & mask & 0b111) == mask) {
        return true;
    }
    return false;
}

// 判断文件名是否相等
bool match_name(const char* name, const char* entry_name, char** next) {
    char* lhs = (char*)name;
    char* rhs = (char*)entry_name;

    // 过滤掉可能的无效分隔符，如传入"//////filename"
    while(IS_SEPARATOR(*lhs)){
        lhs++;
    }
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

/**
 * 获取 dir 目录下的 name 目录所在的 dentry_t 和 cache_t
 * 输入父目录 dir，以及想要查找的目标目录的名称
 * 输出 next 是下一层级的 path，如 name 输入 a/b/c，next 返回 b/c
 * result 是找到的目标目录的 entry
 * 失败返回 NULL
 *  */ 
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
            brelse(pcache);  // 释放之前的缓存
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
            block = bmap(dir, i / BLOCK_DENTRIES,
                         true);  // 创建，为可能的添加做准备
            assert(block);

            pcache = bread(dir->dev, block);
            entry = (dentry_t*)pcache->data;
        }

        // 已扫描一遍 nr 都非 0，增加新的 dentry
        /**
         * todo: 我认为这里需要判断是否大于当前数据块，可能需要为该目录 inode
         * 分配新的数据块，不应直接修改大小。
         */
        if (i * sizeof(dentry_t) >= dir->desc->size) {
            entry->nr = 0;
            dir->desc->size = (i + 1) * sizeof(dentry_t);
            // dir->cache->dirty = true;
        }
        if (entry->nr) {
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

void dir_test() {
    char pathname[] = "/d1/d2////d3/d4/";
    inode_t* inode = namei(pathname);
    if (!inode){
        LOGK("error pathname: %s, no such directory!\n", pathname);
    }else{
        LOGK("get %s inode!\n", pathname);
    }
    iput(inode);

    char pathname2[] = "/d_no";
    inode = namei(pathname2);
    if (!inode){
        LOGK("error pathname: %s, no such directory!\n", pathname2);
    }else{
        LOGK("get %s inode!\n", pathname2);
    }
    iput(inode);

    char pathname3[] = "/home/hello.txt";
    inode = namei(pathname3);
    if (!inode){
        LOGK("error pathname: %s, no such directory!\n", pathname3);
    }else{
        LOGK("get %s inode!\n", pathname3);
    }
    iput(inode);
    
}

inode_t* named(char* pathname, char** next) {
    inode_t* inode = NULL;
    task_t* task = get_current();
    dev_t dev;

    char* left = pathname;
    // 绝对路径与相对路径判断
    if(IS_SEPARATOR(left[0])){
        inode = task->iroot;
        left++;
    }else if (left[0]){
        inode = task->ipwd;
    }else{
        return NULL;
    }

    inode->count++;
    *next = left;

    // 传入的 pathname 是空的
    if (!*left){
        return inode;
    }

    char* right = strrsep(left);
    // 没有最后一级的分隔符了，说明已经找到了父目录 inode
    if (!right || right < left){
        return inode;
    }
    // 开始层级查找 inode->dentry->data
    right++;
    *next = left;
    dentry_t* entry = NULL;
    cache_t * pcache = NULL;
    while (true){
        // 拿到目录下的 entry 和其所在 pcache
        pcache = find_entry(&inode, left, next, &entry);
        // 任何一级没有找到，直接失败
        if (!pcache){
            iput(inode);
            goto failure;
        }
        // 切换到下一级目录
        dev = inode->dev; // 必须更新，防止 mount 到其他设备上。
        iput(inode);
        inode = iget(dev, entry->nr);
        brelse(pcache); // entry 使用后才可以释放其所在的 pcache
        // 下一级非目录，或没有执行权限
        if (!ISDIR(inode->desc->mode) || !permission(inode, P_EXEC)){
            goto failure;
        }
        // 找到最终目标
        if (right == *next){
            goto success;
        }
        left = *next;
    }


success:
    return inode;

failure:
    return NULL;
}

inode_t* namei(char* pathname) {
    char* next = NULL;
    inode_t* dir = named(pathname, &next);
    if (!dir){
        return NULL;
    }
    // 这种情况是传入的 pathname 以分隔符结尾
    if (!(*next)){
        return dir;
    }
    
    char* name = next;
    dentry_t* entry = NULL;
    cache_t* pcache = find_entry(&dir, name, &next, &entry);
    if (!pcache){
        iput(dir);
        return NULL;
    }

    inode_t* inode = iget(dir->dev, entry->nr);
    iput(dir);
    brelse(pcache);

    return inode;    
}