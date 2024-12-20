#ifndef LIGHTOS_FS_H
#define LIGHTOS_FS_H

#include <lib/list.h>
#include <lightos/cache.h>
#include <lightos/stat.h>
#include <sys/types.h>

// defined in cache.h
// #define BLOCK_SIZE 1024  // 块大小
// #define SECTOR_SIZE 512  // 扇区大小

/**
 * MINIX V1，每块 1K，顺序如下
 * | boot | superblock | imap(s) | bmap(s) | inode(s)...
 */
#define FS_BOOT_BLOCK_NR 0
#define FS_SUPER_BLOCK_NR 1
#define FS_IMAP_BLOCK_NR 2

#define MINIX_V1_MAGIC 0X137F  // 文件系统魔数
#define NAME_LEN 14            // 文件名长度

#define IMAP_NR 8  // inode 位图块最大值
#define ZMAP_NR 8  // 块位图块最大值

#define BLOCK_BITS (BLOCK_SIZE * 8)  // 一个块的位图能管理的大小
#define BLOCK_INODES (BLOCK_SIZE / sizeof(inode_desc_t))  // 块 inode 数量
#define BLOCK_DENTRIES (BLOCK_SIZE / sizeof(dentry_t))    // 块 dentry 数量

#define DIRECT_BLOCKS (7)                         // 直接块数量
#define BLOCK_INDEXES (BLOCK_SIZE / sizeof(u16))  // 块索引数量
#define INDIRECT1_BLOCKS BLOCK_INDEXES            // 一级间接块数量
#define INDIRECT2_BLOCKS \
    (INDIRECT1_BLOCKS * INDIRECT1_BLOCKS)  // 二级间接块数量

#define TOTAL_BLOCKS \
    (DIRECT_BLOCKS + INDIRECT1_BLOCKS + INDIRECT2_BLOCKS)  // 全部块数量
// #define FILE_MAX_SIZE (TOTAL_BLOCKS * BLOCK_SIZE)  // 文件最大大小（256MB）
#define FILE_MAX_SIZE (BLOCK_SIZE)  // ulimit 文件最大大小（1KB）

#define MAX_PATH_LEN 4096  // 最大路径长度

#define ACC_MODE(x) ("\004\002\006\377"[(x) & O_ACCMODE])

enum file_flag {
    O_RDONLY = 00,     // 只读方式
    O_WRONLY = 01,     // 只写方式
    O_RDWR = 02,       // 读写方式
    O_ACCMODE = 03,    // 文件访问模式屏蔽码
    O_CREAT = 00100,   // 如果文件不存在就创建
    O_EXCL = 00200,    // 独占使用文件标志
    O_NOCTTY = 00400,  // 不分配控制终端
    O_TRUNC = 01000,   // 若文件已存在且是写操作，则长度截为 0
    O_APPEND = 02000,  // 以添加方式打开，文件指针置为文件尾
    O_NONBLOCK = 04000,  // 非阻塞方式打开和操作文件
};

typedef struct inode_desc_t {
    u16 mode;  // 文件类型和属性(rwx 位)
    u16 uid;   // 用户id（文件拥有者标识符）
    u32 size;  // 文件大小（字节数）
    u32 mtime;  // 修改时间戳 这个时间戳应该用 UTC 时间，不然有瑕疵
    u8 gid;       // 组id(文件拥有者所在的组)
    u8 nlinks;    // 链接数（多少个文件目录项指向该 i 节点）
    u16 zone[9];  // 直接 (0-6)、间接(7)或双重间接 (8) 逻辑块号
} inode_desc_t;

// 内存视图的 inode
typedef struct inode_t {
    inode_desc_t* desc;  // inode 描述符
    cache_t* cache;      // inode 描述符所在cache
    dev_t dev;           // 设备号
    idx_t nr;            // i节点号
    u32 count;           // 引用计数
    time_t atime;        // 访问时间
    time_t ctime;        // 修改时间
    list_node_t node;    // 链表节点
    dev_t mount;         // 安装设备
    task_t* rxwaiter;    // 读等待进程
    task_t* txwaiter;    // 写等待进程
    bool pipe;           // 管道标志
} inode_t;

typedef struct super_desc_t {
    u16 inodes;         // 节点数
    u16 zones;          // 逻辑块数
    u16 imap_blocks;    // i 节点位图所占用的数据块数
    u16 zmap_blocks;    // 逻辑块位图所占用的数据块数
    u16 firstdatazone;  // 第一个数据逻辑块号
    u16 log_zone_size;  // log2(每逻辑块数据块数)
    u32 max_size;       // 文件最大长度
    u16 magic;          // 文件系统魔数
} super_desc_t;

// 内存视图的 super block
typedef struct super_block_t {
    super_desc_t* desc;       // 超级块描述符
    cache_t* cache;           // 超级块描述符所在cache
    cache_t* imaps[IMAP_NR];  // inode位图缓冲
    cache_t* zmaps[ZMAP_NR];  // 块位图缓冲
    dev_t dev;                // 设备号
    u32 count;                // 引用计数
    list_t inode_list;        // 使用中inode链表
    inode_t* iroot;           // 根目录inode
    inode_t* imount;          // 安装到的inode
} super_block_t;

// 文件目录项结构
typedef struct dentry_t {
    u16 nr;         // i 节点
    char name[14];  // 文件名
} dentry_t;

typedef dentry_t dirent_t;

typedef struct file_t {
    inode_t* inode;  // 文件 inode
    u32 count;       // 引用计数
    off_t offset;    // 文件偏移
    int flags;       // 文件标记
    int mode;        // 文件模式
} file_t;

typedef enum whence_t {
    SEEK_SET = 1,  // 直接设置偏移
    SEEK_CUR,      // 当前位置偏移
    SEEK_END       // 结束位置偏移
} whence_t;

#define P_EXEC IXOTH
#define P_WRITE IWOTH
#define P_READ IROTH

// 检查权限
bool permission(inode_t* inode, u16 mask);

// 获取设备 dev 的超级块
super_block_t* get_super(dev_t dev);
// 释放设备 dev 的超级块
void put_super(super_block_t* sb);
// 读设备 dev 的超级块
super_block_t* read_super(dev_t dev);
// syscall: mount/umount
int32 sys_mount(char* devname, char* dirname, int flags);
// umount target 可能是 devname 或 dirname 的其中一个
int32 sys_umount(char* target);
// syscall: mkfs 格式化文件系统
int32 sys_mkfs(char* devname, int icount);
// 格式化文件系统，传入 dev 与预期 inode 数量（默认1/3*blocks，传0）
int32 devmkfs(dev_t dev, u32 icount);
// 初始化
void super_init(void);
void inode_init(void);

// 分配一个文件块
idx_t balloc(dev_t dev);
// 释放一个文件块
void bfree(dev_t dev, idx_t idx);
// 分配一个文件系统 inode
idx_t ialloc(dev_t dev);
// 释放一个文件系统 inode
void ifree(dev_t dev, idx_t idx);

// 获取 inode 第 block 块的索引值
// 如果不存在且 create 为 true，则在 data zone 创建一级/二级索引块
idx_t bmap(inode_t* inode, idx_t block, bool create);

// 获取根目录 inode
inode_t* get_root_inode();
// 获取设备 dev 的 nr inode
inode_t* iget(dev_t dev, idx_t nr);
// 释放 inode
void iput(inode_t* inode);
// iget 的简单封装，获取新的 inode 并填写基本配置
inode_t* new_inode(dev_t dev, idx_t nr);
// 创建管道 inode
inode_t* get_pipe_inode(void);
// 释放管道 inode
void put_pipe_inode(inode_t* inode);

// 判断文件名是否相等
bool match_name(const char* name, const char* entry_name, char** next);
// 获取 dir 目录下的 name 目录 所在的 dentry_t 和 cache_t
cache_t* find_entry(inode_t** dir,
                    const char* name,
                    char** next,
                    dentry_t** result);
// 在 dir 目录中添加 name 目录项
cache_t* add_entry(inode_t* dir, const char* name, dentry_t** result);

// 获取父目录 inode
inode_t* named(char* pathname, char** next);
// 获取目标 inode
inode_t* namei(char* pathname);

// 打开文件，返回 inode
inode_t* inode_open(char* pathname, int flag, int mode);
// 从 inode 的 offset 处，读 len 个字节 到 buf
int inode_read(inode_t* inode, char* buf, u32 len, off_t offset);
// 从 inode 的 ofsset 处，将 buf 的 len 个字节写入磁盘
int inode_write(inode_t* inode, char* buf, u32 len, off_t offset);

// 释放 inode 所有文件块
void inode_truncate(inode_t* inode);

// syscall: 创建目录
int sys_mkdir(char* pathname, int mode);
// syscall: 删除目录
int sys_rmdir(char* pathname);

// syscall: 创建硬链接
int sys_link(char* oldname, char* newname);
// syscall: 删除硬链接
int sys_unlink(char* filename);

/****************************************************
 * 文件相关
 ****************************************************/

// 初始化
void file_init(void);
// 从 file_table 获取一个空的文件指针
file_t* get_file(void);
// 释放文件，减少 inode 引用计数，iput 释放（与 inode_open 对应）
void put_file(file_t* file);
// syscall open/creat/close
fd_t sys_open(char* filename, int flags, int mode);
fd_t sys_creat(char* filename, int mode);
void sys_close(fd_t fd);
// syscall: read/write
int32 sys_read(fd_t fd, char* buf, u32 count);
int32 sys_write(fd_t fd, char* buf, u32 count);
// syscall: lseek
int32 sys_lseek(fd_t fd, off_t offset, whence_t whence);
// syscall: getcwd/chdir/chroot
int32 sys_getcwd(char* buf, size_t size);
int32 sys_chdir(char* pathname);
int32 sys_chroot(char* pathname);
// syscall: readdir。本质上就是将目录 inode 作为一个文件打开和读取 dentry
int32 sys_readdir(fd_t fd, void* dir, int count);
// syscall: dup. dup oldfd to another fd, return new fd
fd_t sys_dup(fd_t oldfd);
// syscall: dup2. dup oldfd to newfd (if newfd is not NULL, will close it
// first), return newfd
fd_t sys_dup2(fd_t oldfd, fd_t newfd);
// 管道
int pipe_read(inode_t* inode, char* buf, int count);
int pipe_write(inode_t* inode, char* buf, int count);
int32 sys_pipe(fd_t pipefd[2]);

/****************************************************
 * 设备相关
 ****************************************************/

// 文件系统设备初始化
void dev_init(void);
// syscall: mknod 创建设备文件
int32 sys_mknod(char* filename, int mode, int dev);

#endif