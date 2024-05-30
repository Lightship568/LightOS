#include <lightos/console.h>
#include <lightos/io.h>
#include <lib/string.h>

#define CRT_ADDR_REG 0x3D4 // CRT(6845)索引寄存器
#define CRT_DATA_REG 0x3D5 // CRT(6845)数据寄存器

#define CRT_START_ADDR_H 0xC // 显示内存起始位置 - 高位
#define CRT_START_ADDR_L 0xD // 显示内存起始位置 - 低位
#define CRT_CURSOR_H 0xE     // 光标位置 - 高位
#define CRT_CURSOR_L 0xF     // 光标位置 - 低位

#define SCREEN_MEM_BASE 0xB8000              // 显卡内存起始位置
#define SCREEN_MEM_SIZE 0x4000               // 显卡内存大小 16k
#define SCREEN_MEM_DIVIDE 0x3000               // 滚屏分割点，滚屏超过MEM_END后拷贝后3/4（0x3000/0x4000）至开头
#define SCREEN_MEM_END (SCREEN_MEM_BASE + SCREEN_MEM_SIZE) // 显卡内存结束位置
#define WIDTH 80                      // 屏幕文本列数
#define HEIGHT 25                     // 屏幕文本行数
#define ROW_SIZE (WIDTH * 2)          // 每行字节数（前一个字节为ascii，后一个为字符样式）
#define SCR_SIZE (ROW_SIZE * HEIGHT)  // 屏幕字节数

#define ASCII_NULL  0x00
#define ASCII_ENQ   0x05
#define ASCII_ESC   0x1B // ESC
#define ASCII_BEL   0x07 // \a
#define ASCII_BS    0x08 // \b
#define ASCII_HT    0x09 // \t
#define ASCII_LF    0x0A // \n
#define ASCII_VT    0x0B // \v
#define ASCII_FF    0x0C // \f
#define ASCII_CR    0x0D // \r
#define ASCII_DEL   0x7F

#define GET_MEM_FROM_OFFSET(offset) ((offset)<<1)+SCREEN_MEM_BASE // dw为一个字符

static u32 screen;  // 当前显示器开始的内存位置
static u32 pos;     // 当前光标的内存位置
static u32 x, y;    // 当前光标的坐标（80x25）
static u8 attr = 7; // 字符样式
static u16 erase = 0x0720;   //删除后清空为空格

// 获得当前指针的开始位置
static void get_screen(){
    outb(CRT_ADDR_REG, CRT_START_ADDR_H);
    screen = inb(CRT_DATA_REG) << 8;
    outb(CRT_ADDR_REG, CRT_START_ADDR_L);
    screen |= inb(CRT_DATA_REG);
    screen = GET_MEM_FROM_OFFSET(screen);
}

// 设置显示器开始的位置
static void set_screen(){
    outb(CRT_ADDR_REG, CRT_START_ADDR_H);
    outb(CRT_DATA_REG, ((screen - SCREEN_MEM_BASE) >> 9) & 0xff);
    outb(CRT_ADDR_REG, CRT_START_ADDR_L);
    outb(CRT_DATA_REG, ((screen - SCREEN_MEM_BASE) >> 1) & 0xff);
}

// 获得光标位置
static void get_cursor(){
    outb(CRT_ADDR_REG, CRT_CURSOR_H);
    pos = inb(CRT_DATA_REG) << 8;
    outb(CRT_ADDR_REG, CRT_CURSOR_L);
    pos |= inb(CRT_DATA_REG);
    x = pos % WIDTH;
    y = pos / WIDTH;
    pos = GET_MEM_FROM_OFFSET(pos);
}

// 设置光标位置
static void set_cursor(){
    outb(CRT_ADDR_REG, CRT_CURSOR_H);
    outb(CRT_DATA_REG, ((pos - SCREEN_MEM_BASE) >> 9) & 0xff);
    outb(CRT_ADDR_REG, CRT_CURSOR_L);
    outb(CRT_DATA_REG, ((pos - SCREEN_MEM_BASE) >> 1) & 0xff);
}

static void scroll_up(){
    if (screen + SCR_SIZE + ROW_SIZE <= SCREEN_MEM_END){
        u16* ptr = (u16*) (screen + SCR_SIZE);
        for (size_t i = 0; i < WIDTH; i++) {
            *ptr++ = erase;
        }
        screen += ROW_SIZE;
        set_screen();
    }else{
        // 以1/4为分割，拷贝后3/4到开头，覆盖前1/4
        // 简单观察过结果，应该没问题
        memcpy((void *)SCREEN_MEM_BASE, (void *)(SCREEN_MEM_BASE + (SCREEN_MEM_SIZE - SCREEN_MEM_DIVIDE)), SCREEN_MEM_DIVIDE);
        screen -= SCREEN_MEM_SIZE - SCREEN_MEM_DIVIDE; //差值
        pos -= SCREEN_MEM_SIZE - SCREEN_MEM_DIVIDE;
        u16* ptr = (u16*) (screen + SCR_SIZE);
        for (size_t i = 0; i < WIDTH; i++) {
            *ptr++ = erase;
        }
        screen += ROW_SIZE;
        set_screen();
    }
}

static void scroll_down(){
    screen -= ROW_SIZE;
    set_screen();
}

// 计算 xy 位置
static void set_xy(){
    u32 tmp = (pos - screen) >> 1;
    x = tmp % WIDTH;
    y = tmp / WIDTH;
    // 仅考虑因输出导致的屏幕滚动
    if (y >= HEIGHT){
        do {
            scroll_up();
        } while (--y >= HEIGHT);
    }
}

void console_clear(){
    screen = SCREEN_MEM_BASE;
    pos = SCREEN_MEM_BASE;
    x = y = 0;
    set_screen();
    set_cursor();
    
    u16 *ptr = (u16*) SCREEN_MEM_BASE;
    while(ptr < (u16 *)SCREEN_MEM_END){
        *ptr++ = erase;
    }
}

void console_write(char *buf, u32 count){
    if (count == -1){
        count = strlen(buf);
    }
    char ch;
    while (count--){
        ch = *buf++;
        switch (ch)
        {
        case ASCII_NULL:

            break;
        case ASCII_ENQ:

            break;
        case ASCII_ESC:

            break;
        case ASCII_BEL: //a
            // todo bell声音
            break;
        case ASCII_BS:  //backspace
            if (x){
                x--;
                pos -= 2;
                *(u16*)pos = erase;
            }
            break;
        case ASCII_HT:  //t HT (Horizontal Tab)

            break;
        case ASCII_LF:  //n LF/NL(Line Feed/New Line)
            pos += (WIDTH - x) << 1;
            set_xy();
            break;
        case ASCII_VT:  //v VT (Vertical Tab)

            break;
        case ASCII_FF:  //f FF/NP (Form Feed/New Page)

            break;
        case ASCII_CR:  //r CR (Carriage Return)
            pos -= (x << 1);
            break;
        case ASCII_DEL:
            *(u16*)pos = erase;
            break;
        default:
            *(u8 *)pos = ch;
            *((u8 *)pos+1) = attr;
            pos += 2;
            set_xy();
            break;
        }
        set_cursor();
    }
}

void console_init(){
    console_clear(); 
    
}