#ifndef LIGHTOS_KFIFO_H
#define LIGHTOS_KFIFO_H

/**
 * | ---------- | ----------- | ----------- | .... loop back
 *      emtpy     xxxxxxxxxxx ->   empty
 *          tail(out)      head(in)
 */

#include <sys/types.h>

typedef struct kfifo_t{
    u32 head;     // input
    u32 tail;    // output
    u32 buf_size;   // buf_size
    char *buf;  // buffer
} kfifo_t;

void kfifo_init(kfifo_t* fifo, char* buf, u32 buf_size);
// check if a fifo buffer is full
bool kfifo_full(kfifo_t* fifo);
// check if a fifo buffer is empty
bool kfifo_empty(kfifo_t* fifo);

// if a fifo buffer is empty, return NULL
char kfifo_get(kfifo_t* fifo);

// if a fifo buffer is full, overwrite the head item
void kfifo_put(kfifo_t* fifo, char byte);

#endif