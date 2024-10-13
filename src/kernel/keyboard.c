#include <lightos/interrupt.h>
#include <lib/io.h>
#include <lib/print.h>
#include <sys/assert.h>

#define KEYBOARD_DATA_PROT 0x60
#define KEYBOARD_CTRL_PORT 0x64

void keyboard_handler(int vector){
    assert(vector == 0x21);
    send_eoi(vector);
    u16 scancode = inb(KEYBOARD_DATA_PROT);
    printk("Keyboard input %d\n", scancode);
}

void keyboard_init(void){
    set_interrupt_handler(IRQ_KEYBOARD, keyboard_handler);
    set_interrupt_mask(IRQ_KEYBOARD, true);
}
