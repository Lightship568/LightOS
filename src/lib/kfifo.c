#include <lib/kfifo.h>

static _inline u32 kfifo_next(kfifo_t* fifo, u32 pos){
    return (pos + 1) % fifo->buf_size;
}

void kfifo_init(kfifo_t* fifo, char* buf, u32 buf_size){
    fifo->buf = buf;
    fifo->buf_size = buf_size;
    fifo->head = 0;
    fifo->tail = 0;
}

bool kfifo_full(kfifo_t* fifo){
    return kfifo_next(fifo, fifo->head) == fifo->tail;
}

bool kfifo_empty(kfifo_t* fifo){
    return fifo->head == fifo->tail;
}

char kfifo_get(kfifo_t* fifo){
    if (kfifo_empty(fifo)) return 0;
    char byte = fifo->buf[fifo->tail];
    fifo->tail = kfifo_next(fifo, fifo->tail);
    return byte;
}

void kfifo_put(kfifo_t* fifo, char byte){
    while (kfifo_full(fifo)){ // 如果队列已满，丢弃队列尾部
        kfifo_get(fifo);
    }
    fifo->buf[fifo->head] = byte;
    fifo->head = kfifo_next(fifo, fifo->head);
}
