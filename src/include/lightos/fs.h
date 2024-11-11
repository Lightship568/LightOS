#ifndef LIGHTOS_FS_H
#define LIGHTOS_FS_H

#include <lib/list.h>
#include <lightos/cache.h>
#include <sys/types.h>

// defined in cache.h
// #define BLOCK_SIZE 1024  // 块大小
// #define SECTOR_SIZE 512  // 扇区大小

#define MINIX_V1_MAGIC 0X137F  // 文件系统魔数
#define NAME_LEN 14            // 文件名长度

#define IMAP_NR 8  // inode 位图块最大值
#define ZMAP_NR 8  // 块位图块最大值

typedef struct inode_desc_t {
    u16 mode;  // 文件类型和属性(rwx 位)
    u16 uid;   // 用户id（文件拥有者标识符）
    u32 size;  // 文件大小（字节数）
    u32 mtime;  // 修改时间戳 这个时间戳应该用 UTC 时间，不然有瑕疵
    u8 gid;       // 组id(文件拥有者所在的组)
    u8 nlinks;    // 链接数（多少个文件目录项指向该 i 节点）
    u16 zone[9];  // 直接 (0-6)、间接(7)或双重间接 (8) 逻辑块号
} inode_desc_t;

// 内存视图的inode
typedef struct inode_t {
    inode_desc_t* desc;  // inode 描述符
    cache_t* cache;      // inode 描述符所在cache
    dev_t dev;           // 设备号
    idx_t nr;            // i节点号
    u32 count;           // 引用计数
    time_t atime;        // 访问时间
    time_t ctime;        // 创建时间
    list_node_t node;    // 链表节点
    dev_t mount;         // 安装设备

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

typedef struct super_block_t {
    super_desc_t* desc;       // 超级块描述符
    cache_t* cache;           // 超级块描述符所在cache
    cache_t* imaps[IMAP_NR];  // inode位图缓冲
    cache_t* zmaps[ZMAP_NR];  // 块位图缓冲
    dev_t dev;                // 设备号
    list_t inode_list;        // 使用中inode链表
    inode_t* iroot;           // 根目录inode
    inode_t* imount;          // 安装到的inode
} super_block_t;

// 文件目录项结构
typedef struct dentry_t {
    u16 nr;         // i 节点
    char name[14];  // 文件名
} dentry_t;

#endif