#ifndef LIGHTOS_VSPRINTF_H
#define LIGHTOS_VSPRINTF_H

#include <sys/stdarg.h>

int vsprintf(char *buf, const char *fmt, va_list args);
int sprintf(char *buf, const char *fmt, ...);
// int printf(const char *fmt, ...);

#endif