#ifndef LIGHTOS_PRINTK_H
#define LIGHTOS_PRINTK_H

/**
 * 内核printk函数包装，为了避免与c的用户空间库混淆，没起名为stdio
 * 底层调用的 vsprintf 与 console_write
*/

int printk(const char* fmt, ...);

#endif