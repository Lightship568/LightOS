SECTIONS
{
  /* 设置虚拟地址起始点 */
  . = 0xC0010000;

  /* 定义一个变量来跟踪物理地址 */
  __physical_base = 0x10000;  /* 物理地址从 0x10000 开始 */


  .multiboot2 : AT(__physical_base) 
  {
      KEEP(*(.multiboot2))
  }

  __physical_base = 0x10040; /* .text 物理地址从 0x10040 开始 */
  . = 0xC0010040;

  .text : AT(__physical_base)
  {
      KEEP(*(.text))
  }

  .rodata ALIGN(4K): 
  {
    KEEP(*(.rodata))
  }

 
  .eh_frame ALIGN(4K): 
  {
    KEEP(*(.eh_frame))
  }

  .data ALIGN(4K): 
  {
    *(.data)
  }

  .bss : 
  {
    *(.bss)
  }
}
