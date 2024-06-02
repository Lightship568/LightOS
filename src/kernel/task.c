#include <lightos/task.h>
#include <sys/global.h>
#include <lib/print.h>
#include <lib/stdlib.h>
#include <sys/interrupt.h>


#define task1_base 0x1000
#define task2_base 0x2000

extern void switch_to(void *eip, task_t next);

task_t* task_list[2];
task_t *current = (task_t *)NULL;


void task_schedule(void){
    if (current && current == task_list[0]){
        current = task_list[1];
        switch_to(current->tss.eip, *current);
    }else{
        current = task_list[0];
        switch_to(current->tss.eip, *current);
    }
}
void task_2(void);
void task_1(void){
    start_interrupt();
    while(true){
        printk("A");
        delay(100000);
    }
}

void task_2(void){
    start_interrupt();
    while(true){
        printk("B");
        delay(100000);
    }

}

void task_create(task_t **task, u32 *base, void* func){
    u32 stack = (u32)base + PAGE_SIZE;
    task_t* task_pcb = (task_t*)base; 
    task_pcb->base = (u32*)base;
    task_pcb->stack = (u32*)stack;
    task_pcb->tss.ebx = 0x11111111;
    task_pcb->tss.edi = 0x22222222;
    task_pcb->tss.esi = 0x33333333;
    task_pcb->tss.ebp = 0x44444444;
    task_pcb->tss.eip = func;

    *task = task_pcb;
}

void task_test_init(void){
    task_create(&task_list[0], (u32*)task1_base, task_1);
    task_create(&task_list[1], (u32*)task2_base, task_2);
}