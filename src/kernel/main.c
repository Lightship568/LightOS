# include <lightos/lightos.h>

int magic = LIGHTOS_MAGIC;
char message[] = "hello LightOS!!!!!!!";
char buf[1024];

void kernel_init(){
    char *video = (char*) 0xb8000; //文本显示器内存位置
    for (int i = 0; i <sizeof(message); i++){
        video[i*2] = message[i];
    }
}