#ifndef LIGHTOS_VSPRINTF_H
#define LIGHTOS_VSPRINTF_H

/**
 * 底层字符串解析库函数
 * 修改自已有开源代码库、onix做了汇编转c的工作
*/

#include <sys/stdarg.h>

int vsprintf(char *buf, const char *fmt, va_list args);
int sprintf(char *buf, const char *fmt, ...);
// int printf(const char *fmt, ...);

#endif