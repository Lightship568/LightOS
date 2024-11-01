#ifndef LIGHTOS_STRING_H
#define LIGHTOS_STRING_H

/**
 * 字符串处理库
 * 参考 c++ 标准库实现：https://en.cppreference.com/w/c/string/byte
 * 内核使用 
*/

#include <sys/types.h>

char *strcpy(char *dest, const char *src);
// 添加字符串EOS
char *strncpy(char *dest, const char *src, size_t count);
char *strcat(char *dest, const char *src);
size_t strlen(const char *str);
size_t strnlen(const char *str, size_t maxlen);
int strcmp(const char *lhs, const char *rhs);
char *strchr(const char *str, int ch);
char *strrchr(const char *str, int ch);
char *strsep(const char *str);
char *strrsep(const char *str);

int memcmp(const void *lhs, const void *rhs, size_t count);
void *memset(void *dest, int ch, size_t count);
void *memcpy(void *dest, const void *src, size_t count);
void *memchr(const void *ptr, int ch, size_t count);

#endif