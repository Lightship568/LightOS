/**
 * 基础类型定义
 * 方便后续开发
*/

# ifndef LIGHTOS_TYPES_H
# define LIGHTOS_TYPES_H

#include <lightos/lightos.h>

#define EOF -1 // END OF FILE

#define NULL ((void *)0) // 空指针

#define EOS '\0' // 字符串结尾

#define CONCAT(x, y) x##y
#define RESERVED_TOKEN(x, y) CONCAT(x, y)
#define RESERVED RESERVED_TOKEN(reserved, __LINE__)

#ifndef __cplusplus
    #define bool _Bool
    #define true 1
    #define false 0
#endif

// 标记弱符号
#define weak __attribute__((__weak__))

// 标记不会返回的函数
#define noreturn __attribute__((__noreturn__))

// 结构体压缩
#define _packed __attribute__((packed)) 

// 用于省略函数的栈帧
#define _ofp __attribute__((optimize("omit-frame-pointer")))
// 强制 inline
#define _inline __attribute__((always_inline)) inline

typedef unsigned int size_t;
typedef char int8;
typedef short int16;
typedef int int32;
typedef long long int64;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef u32 time_t;
typedef u32 idx_t;

typedef u32 pid_t;

typedef u32 dev_t;

typedef u16 mode_t;

typedef int32 fd_t; // 文件描述符索引，负值表示错误码
typedef enum std_fd_t{
    STDIN_FILENO,
    STDOUT_FILENO,
    STDERR_FILENO,
} std_fd_t;
typedef int32 off_t; // 文件偏移

#endif
