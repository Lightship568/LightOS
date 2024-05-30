/**
 * main.c
*/

#include <lightos/lightos.h>
#include <lightos/types.h>
#include <lightos/io.h>
#include <lightos/console.h>

char message[] = "hello LightOS!!!111111111111111111111111111111111111111111111\n\0";


void kernel_init(){
    console_init(); 
    for (size_t i = 0; i < 99999; i++)
    {
        console_write(message, -1);
    }
    
    
    // console_write(message, sizeof(message));
    while(true);
    return;
} 