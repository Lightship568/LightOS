#include <lib/debug.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <lightos/fs.h>
#include <lightos/stat.h>
#include <sys/assert.h>
#include <sys/types.h>

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
    while (IS_SEPARATOR(*lhs)) {
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

    // 针对 mount 下根目录访问 .. 的处理
    if (match_name(name, "..", next) && (*dir)->nr == 1 && (*dir) != get_root_inode()){
        super_block_t* sb = get_super((*dir)->dev);
        inode_t* tmpi = *dir;
        (*dir) = sb->imount;
        (*dir)->count++;
        iput(tmpi);
        put_super(sb);
    }

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
        if (entry->nr == 0){
            continue;
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
            // 创建，为可能的添加做准备
            block = bmap(dir, i / BLOCK_DENTRIES, true);
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

inode_t* named(char* pathname, char** next) {
    inode_t* inode = NULL;
    task_t* task = get_current();
    dev_t dev;

    if (!pathname){
        goto failure;
    }

    char* left = pathname;
    // 绝对路径与相对路径判断
    if (IS_SEPARATOR(left[0])) {
        inode = task->iroot;
        left++;
    } else if (left[0]) {
        inode = task->ipwd;
    } else {
        return NULL;
    }

    inode->count++;
    *next = left;

    // 传入的 pathname 是空的
    if (!*left) {
        return inode;
    }

    char* right = strrsep(left);
    // 没有最后一级的分隔符了，说明已经找到了父目录 inode
    if (!right || right < left) {
        return inode;
    }
    // 开始层级查找 inode->dentry->data
    right++;
    *next = left;
    dentry_t* entry = NULL;
    cache_t* pcache = NULL;
    while (true) {
        // 拿到目录下的 entry 和其所在 pcache
        pcache = find_entry(&inode, left, next, &entry);
        // 任何一级没有找到，直接失败
        if (!pcache) {
            iput(inode);
            goto failure;
        }
        // 切换到下一级目录
        dev = inode->dev;  // 必须更新，防止 mount 到其他设备上。
        iput(inode);
        inode = iget(dev, entry->nr);
        brelse(pcache);  // entry 使用后才可以释放其所在的 pcache
        // 下一级非目录，或没有执行权限
        if (!ISDIR(inode->desc->mode) || !permission(inode, P_EXEC)) {
            goto failure;
        }
        // 找到最终目标
        if (right == *next) {
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
    if (!dir) {
        return NULL;
    }
    // 这种情况是传入的 pathname 以分隔符结尾
    if (!(*next)) {
        return dir;
    }

    char* name = next;
    dentry_t* entry = NULL;
    cache_t* pcache = find_entry(&dir, name, &next, &entry);
    if (!pcache) {
        iput(dir);
        return NULL;
    }

    inode_t* inode = iget(dir->dev, entry->nr);
    iput(dir);
    brelse(pcache);

    return inode;
}

inode_t* inode_open(char* pathname, int flag, int mode) {
    inode_t* dir = NULL;
    inode_t* inode = NULL;
    cache_t* pcache = NULL;
    dentry_t* entry = NULL;
    char* next = NULL;

    // 错误的请求：截断+只读
    if ((flag & O_TRUNC) && ((flag & O_ACCMODE) == O_RDONLY)) {
        goto clean;
    }

    dir = named(pathname, &next);
    if (!dir) {
        goto clean;
    }
    if (!*next) {
        // 传入空 pathname，打开的是 cwd（如单独的 "ls" 命令）
        return dir;
    }

    char* name = next;
    pcache = find_entry(&dir, name, &next, &entry);
    // 找到该文件
    if (pcache) {
        inode = iget(dir->dev, entry->nr);
        if (!permission(inode, flag & O_ACCMODE)){
            goto makeup;
        }
        // 目录可以只读 open
        if (ISDIR(inode->desc->mode) && ((flag & O_ACCMODE) != O_RDONLY)) {
            goto clean;
        }
        goto makeup;
    }

    // 没有该文件且不创建
    if (!(flag & O_CREAT)) {
        goto clean;
    }

    // 准备创建该文件
    if (!permission(dir, P_WRITE)) {
        goto clean;
    }

    // 添加新目录项，申请并获取新文件 inode
    pcache = add_entry(dir, name, &entry);
    entry->nr = ialloc(dir->dev);
    inode = new_inode(dir->dev, entry->nr);

    // 设置 inode
    task_t* task = get_current();
    mode &= (0777 & ~task->umask);
    mode |= IFREG;
    inode->desc->mode = mode;

makeup:
    if (flag & O_TRUNC) {
        inode_truncate(inode);
    }
    brelse(pcache);
    iput(dir);
    return inode;

clean:
    brelse(pcache);
    iput(inode);
    iput(dir);
    return NULL;
}

int inode_read(inode_t* inode, char* buf, u32 len, off_t offset) {
    // 只能读文件或者目录
    assert(ISFILE(inode->desc->mode) || ISDIR(inode->desc->mode));

    // 如果偏移量超过文件大小，返回 EOF
    if (offset >= inode->desc->size) {
        return EOF;
    } else if (len == 0) {
        return 0;
    }

    // 开始读取的位置
    u32 begin = offset;
    // 剩余字节数
    u32 left = MIN(len, inode->desc->size - offset);

    // 开始分块读取
    while (left) {
        // 找到对应的文件偏移，所在的文件块
        idx_t nr = bmap(inode, offset / BLOCK_SIZE, false);
        assert(nr);

        // 读取文件块缓冲
        cache_t* pcache = bread(inode->dev, nr);

        // 文件块内部起始偏移量
        u32 start = offset % BLOCK_SIZE;

        // 本次需要读取的字节数
        u32 chars = MIN(BLOCK_SIZE - start, left);

        // 更新偏移量和剩余字节数
        offset += chars;
        left -= chars;

        // 文件块中的指针
        char* ptr = pcache->data + start;

        // 拷贝内容
        memcpy(buf, ptr, chars);

        // 更新缓存位置
        buf += chars;

        // 收尾释放该块
        brelse(pcache);
    }
    // 更新访问时间
    inode->atime = sys_time();
    // 返回读取字节数
    return offset - begin;
}

int inode_write(inode_t* inode, char* buf, u32 len, off_t offset) {
    // 不允许写入目录，目录有其他专用方法
    assert(ISFILE(inode->desc->mode));

    // todo: 需要做空间上限管理
    if (len + offset >= FILE_MAX_SIZE) {
        return EOF;
    }

    // 开始写入的位置
    u32 begin = offset;
    // 剩余字节数
    u32 left = len;

    // 开始分块写入
    while (left) {
        // 找到对应的文件偏移，所在的文件块，不能存在则需要创建
        idx_t nr = bmap(inode, offset / BLOCK_SIZE, true);
        assert(nr);

        // 读取文件块缓冲
        cache_t* pcache = bread(inode->dev, nr);

        // 文件块内部起始偏移量
        u32 start = offset % BLOCK_SIZE;

        // 本次需要写入的字节数
        u32 chars = MIN(BLOCK_SIZE - start, left);

        // 更新偏移量和剩余字节数
        offset += chars;
        left -= chars;

        // 文件块中的指针
        char* ptr = pcache->data + start;

        // 如果偏移量大于文件目前的大小，则需要更新文件大小信息
        if (offset > inode->desc->size) {
            inode->desc->size = offset;
            // inode->cache->dirty = true;
        }

        // 拷贝内容
        memcpy(ptr, buf, chars);

        // 更新缓存位置
        buf += chars;

        // 收尾释放该块
        pcache->dirty = true;
        brelse(pcache);
    }
    // 更新访问和修改时间
    inode->ctime = inode->atime = sys_time();
    inode->desc->mtime = inode->ctime;  // todo： 放入iput中更新

    inode->cache->dirty = true;
    bwrite(inode->cache);  // 强一致

    // 返回读取字节数
    return offset - begin;
}

int sys_mkdir(char* pathname, int mode) {
    char* next = NULL;
    cache_t* entry_cache = NULL;
    inode_t* dir = named(pathname, &next);
    char* name = next;
    dentry_t* entry = NULL;

    // 父目录不存在
    if (!dir) {
        goto clean;
    }
    // 目录名为空
    if (!*next) {
        goto clean;
    }
    // 父目录无写权限
    if (!permission(dir, P_WRITE)) {
        goto clean;
    }

    entry_cache = find_entry(&dir, name, &next, &entry);
    // 已经存在同名目录或文件
    if (entry_cache) {
        goto clean;
    }

    // 在父 inode zone 下新增目录项
    entry_cache = add_entry(dir, name, &entry);
    entry_cache->dirty = true;
    entry->nr = ialloc(dir->dev);

    task_t* task = get_current();

    // 拿到新增的目录 inode
    inode_t* inode = new_inode(dir->dev, entry->nr);
    inode->desc->mode = (mode & 0777 & ~task->umask) | IFDIR;
    inode->desc->nlinks = 2;  // 一个是父目录的 dentry，另一个是本目录的 '.'
    inode->desc->size = sizeof(dentry_t) * 2;

    // '..' 使得父目录链接数 + 1
    dir->desc->nlinks++;
    // 此外 add_entry 已经修改了 dir 的 mtime 和 dirty 位，just in case...
    dir->cache->dirty = true;

    // 写入 inode 目录中的默认目录项
    cache_t* zone_cache = bread(inode->dev, bmap(inode, 0, true));
    zone_cache->dirty = true;

    entry = (dentry_t*)zone_cache->data;
    strcpy(entry->name, ".");
    entry->nr = inode->nr;

    entry++;
    strcpy(entry->name, "..");
    entry->nr = dir->nr;

    // 释放两个 inode 和 两个 cache
    iput(inode);
    iput(dir);

    brelse(zone_cache);
    brelse(entry_cache);

    return 0;

clean:
    // 清理 inode 和 可能的 entry_cache
    iput(dir);
    brelse(entry_cache);
    return EOF;
}

// 删除目录前需要保证目录内容是空的
static bool is_dir_empty(inode_t* inode) {
    assert(ISDIR(inode->desc->mode));
    int entries = inode->desc->size / sizeof(dentry_t);
    if (entries < 2 || !inode->desc->zone[0]) {
        LOGK("Error! Bad directory on dev %d\n", inode->dev);
        return false;
    }

    idx_t i = 0;
    idx_t block = 0;
    cache_t* pcache = NULL;
    dentry_t* entry = NULL;
    int count = 0;

    // 依次读取每个 entry，检查 nr 是否为 0
    for (; i < entries; i++, entry++) {
        if (!pcache || (u32)entry >= (u32)pcache->data + BLOCK_SIZE) {
            brelse(pcache);
            block = bmap(inode, 1 / BLOCK_DENTRIES, false);
            assert(block);

            pcache = bread(inode->dev, block);
            entry = (dentry_t*)pcache->data;
        }
        if (entry->nr) {
            count++;
        }
    }

    brelse(pcache);
    if (count < 2) {
        LOGK("Error! Bad directory on dev %d\n", inode->dev);
        return false;
    }
    return count == 2;
}

int sys_rmdir(char* pathname) {
    char* next = NULL;
    cache_t* entry_cache = NULL;
    inode_t* dir = named(pathname, &next);
    inode_t* inode = NULL;
    char* name = next;
    dentry_t* entry = NULL;
    int ret = EOF;

    // 父目录不存在
    if (!dir) {
        goto clean;
    }
    // 目录名为空
    if (!*next) {
        goto clean;
    }
    // 目录为'.'或'..'
    if (!strcmp(name, ".") || !strcmp(name, "..")) {
        goto clean;
    }
    // 父目录无写权限
    if (!permission(dir, P_WRITE)) {
        goto clean;
    }

    entry_cache = find_entry(&dir, name, &next, &entry);
    // 目录项不存在
    if (!entry_cache) {
        goto clean;
    }

    inode = iget(dir->dev, entry->nr);
    assert(inode);

    // 目录没有硬链接，并且校验了'..'，因此下面的情况应该不会发生
    assert(inode != dir);

    if (!ISDIR(inode->desc->mode)) {
        goto clean;
    }

    task_t* task = get_current();
    // 受限删除且删除用户非目录拥有者
    if ((dir->desc->mode & ISVTX) && task->uid != inode->desc->uid) {
        goto clean;
    }

    // mount 目录，或引用计数不为 0
    if (dir->dev != inode->dev || inode->count > 1) {
        goto clean;
    }

    // 目录必须为空
    if (!is_dir_empty(inode)) {
        goto clean;
    }

    assert(inode->desc->nlinks == 2);

    // 释放 inode 和 dentry
    inode_truncate(inode);
    ifree(inode->dev, inode->nr);

    inode->cache->dirty = true;
    inode->desc->nlinks = 0;
    inode->nr = 0;

    // 父目录修改
    dir->desc->nlinks--;
    dir->ctime = dir->atime = dir->desc->mtime = sys_time();
    dir->cache->dirty = true;
    assert(dir->desc->nlinks > 0);  // 根目录只有'.'指向自己，因此 nlinks >= 1

    entry->nr = 0;
    entry_cache->dirty = true;

    ret = 0;

clean:
    // 清理
    iput(inode);
    iput(dir);
    brelse(entry_cache);
    return ret;
}

int sys_link(char* oldname, char* newname) {
    int ret = EOF;
    cache_t* pcache = NULL;
    inode_t* dir = NULL;
    inode_t* inode = namei(oldname);

    if (!inode) {
        goto clean;
    }
    // 目录不可添加硬链接
    if (ISDIR(inode->desc->mode)) {
        goto clean;
    }

    char* next = NULL;
    dir = named(newname, &next);
    if (!dir) {
        goto clean;
    }
    if (!(*next)) {
        goto clean;
    }
    // mount 的文件不可添加硬链接
    if (dir->dev != inode->dev) {
        goto clean;
    }
    if (!permission(dir, P_WRITE)) {
        goto clean;
    }

    char* name = next;
    dentry_t* entry;

    pcache = find_entry(&dir, name, &next, &entry);
    // 目录项已经存在
    if (pcache) {
        goto clean;
    }

    // 在 newname 父目录 dir 增加 entry
    pcache = add_entry(dir, name, &entry);
    entry->nr = inode->nr;
    pcache->dirty = true;

    inode->desc->nlinks++;
    inode->ctime = sys_time();
    inode->cache->dirty = true;
    ret = 0;

clean:
    brelse(pcache);
    iput(dir);
    iput(inode);
    return ret;
}

int sys_unlink(char* filename) {
    int ret = EOF;
    cache_t* pcache = NULL;
    char* next = NULL;
    inode_t* dir = named(filename, &next);
    inode_t* inode = NULL;

    if (!dir) {
        goto clean;
    }
    if (!(*next)) {
        goto clean;
    }
    if (!permission(dir, P_WRITE)) {
        goto clean;
    }

    char* name = next;
    dentry_t* entry;

    pcache = find_entry(&dir, name, &next, &entry);
    // 目录项不存在
    if (!pcache) {
        goto clean;
    }

    inode = iget(dir->dev, entry->nr);
    // 目标是个目录，一定不是硬链接
    if (ISDIR(inode->desc->mode)) {
        goto clean;
    }
    task_t* task = get_current();
    if ((inode->desc->mode & ISVTX) && task->uid != inode->desc->uid) {
        goto clean;
    }

    // 这种情况应该不可能发生，但是 linux0.11 比较保守
    // nlinks 为 0 代表之前已经被删除了，而该 nr 仍能通过某个目录的 entry
    // 索引到，应该是个bug if (!inode->desc->nlinks){
    //     LOGK("Deleting non exists file (%04x:%d)\n", inode->dev, inode->nr);
    //     inode->desc->nlinks = 1;
    // }
    assert(inode->desc->nlinks);

    entry->nr = 0;
    pcache->dirty = true;

    inode->desc->nlinks--;
    inode->cache->dirty = true;

    // 硬链接清零，删除文件并释放该文件 inode
    if (inode->desc->nlinks == 0) {
        inode_truncate(inode);
        ifree(inode->dev, inode->nr);
    }
    ret = 0;

clean:
    brelse(pcache);
    iput(inode);
    iput(dir);
    return ret;
}

int32 sys_mknod(char*filename, int mode, int dev){
    char* next = NULL;
    inode_t* dir = NULL;
    cache_t* pcache = NULL;
    inode_t* inode = NULL;
    int ret = EOF;

    dir = named(filename, &next);
    if (!dir){
        goto clean;
    }
    if (!*next){
        goto clean;
    }
    if (!permission(dir, P_WRITE)){
        goto clean;
    }

    char*name = next;
    dentry_t* entry;
    pcache = find_entry(&dir, name, &next, &entry);
    // 目录项已经存在
    if (pcache){    
        goto clean;
    }

    pcache = add_entry(dir, name, &entry);
    pcache->dirty = true;
    entry->nr = ialloc(dir->dev);

    inode = new_inode(dir->dev, entry->nr);
    inode->desc->mode = mode;
    
    if (ISBLK(mode) || ISCHR(mode)){
        inode->desc->zone[0] = dev;
    }
    ret = 0;

clean:
    brelse(pcache);
    iput(inode);
    iput(dir);
    return ret;
}

void dir_test() {
    char pathname[] = "/d1/d2////d3/../../../hello.txt";
    inode_t* inode = namei(pathname);

    inode_truncate(inode);
    iput(inode);
}