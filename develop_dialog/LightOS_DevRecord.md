# LightOS Dev Record

LightOS 操作系统开发日志

---

指导 @踌躇月光，https://www.bilibili.com/video/BV1Wr4y1i7kq/。

---

# 常见 BUG 与处理记录

## VSCode相关

* 代码格式化时，大括号不换行：setting 中搜索 C_Cpp.clang_format_style，设置为：{ BasedOnStyle: Chromium, IndentWidth: 4}

## 头文件链接时重复定义

在C/C++编程中，使用`#ifndef`、`#define`和`#endif`来防止头文件被多次包含是一个常见的做法。这被称为“include guard”。`#pragma once`也可以实现相同的效果。这些技术能够防止编译期间的重复定义错误，但它们不能防止链接期间的重复定义错误。

因此.h中只能使用函数声明与extern变量声明，并在.c中实现。并且，**重复构建一定要先clean！！！！make不会自动更新已经构建好的东西！！！**

## PF 页错误

设计中，0 地址并未映射，这样可以对空指针访问进行排查。

如果代码运行突然出现PF，估计就是访问了空指针。

## GP General Protection 错误

爆栈，可能是 esp > ebp



# 开发准备

环境配置

* WSL + Bochs + vsCode

### bochs安装需要图形化界面
    apt install bochs
    apt install bochs-x
    安装XLaunch
    设置~/.bash中DISPLAY=localhost:0.0
    使用xclocks测试成功

补充，在wsl2中由于网络走的net映射，因此在wsl2中配置如下：
[教程](https://aalonso.dev/blog/how-to-use-gui-apps-in-wsl2-forwarding-x-server-cdj)
在WSL2的 ~/.bashrc或 ~/.zshrc文件中添加
`export DISPLAY=172.30.160.1:0.0`（换成windows中wsl网卡ip）
或者自动获取`export DISPLAY=$(ip route|awk '/^default/{print $3}'):0.0`
注意一定要打开防火墙通信请求（控制面板\系统和安全\Windows Defender 防火墙\允许的应用，VcXsrv的私有网络访问）

---

### 汇编
`nasm -f bin boot.asm -o boot.bin`

- **nasm**: 这是命令行上调用NASM汇编器的方式。NASM用于将汇编语言源代码转换成机器语言代码。
- **-f bin**: 这个选项指定了输出文件的格式。在这个例子中，`-f bin`告诉NASM生成一个纯二进制格式的文件（binary format）。这种格式没有任何元数据或重定位信息，直接映射到内存中就可以执行，非常适合用作引导扇区或其他需要直接硬件级别访问的场合。
- **boot.asm**: 这是输入文件的名称，即包含汇编源代码的文件。在这个例子中，`boot.asm`很可能包含了一个操作系统的引导加载程序的代码。
- **-o boot.bin**: 这个选项指定了输出文件的名称。在这里，`-o boot.bin`命令告诉NASM将编译后的二进制结果保存为`boot.bin`。这个文件包含了编译后的机器代码，可以被写入磁盘的引导扇区或用于其他直接执行的环境。

### 生成硬盘镜像文件

`bximage -q -hd=16 -mode=create -sectsize=512 -imgmode=flat LightOS.img`

​	`bximage`是Bochs虚拟机附带的一个工具，用于创建虚拟硬盘镜像文件。这个命令行工具特别有用于准备虚拟环境，比如操作系统开发时用于模拟真实的硬件环境。让我们逐一解释你提供的命令中的各个参数：

- **-q**: 这个参数代表“quiet”模式，意味着`bximage`在执行时会减少输出的信息量，只显示最关键的信息，或者可能根本不显示任何东西，除非发生错误。
- **-hd=16**: 这个参数指定创建的虚拟硬盘大小为16MB。`-hd`选项用于定义硬盘的大小，后面跟的数字代表硬盘的容量，单位为MB。
- **-mode=create**: 这个参数指明`bximage`的操作模式为“创建”模式。这意味着命令的目的是创建一个新的镜像文件，而不是修改或检查一个现有的镜像。
- **-sectsize=512**: 这个参数设置每个扇区（sector）的大小为512字节。扇区是硬盘存储的基本单位，512字节是最常见的扇区大小，尽管现代硬盘可能使用更大的扇区尺寸，如4096字节。
- **-imgmode=flat**: 这个参数指定镜像文件的模式为“flat”模式，意味着创建的是一个平坦的镜像文件，没有分区表或任何复杂结构。这种模式特别适合于简单的操作系统引导或其他需要直接访问硬盘的场合。
- **LightOS.img**: 这是要创建的虚拟硬盘镜像文件的名称。在这个例子中，文件名被指定为`LightOS.img`。这个文件会根据前面的参数配置被创建。

### 将编译好的内核镜像写入硬盘镜像文件

`dd if=boot.bin of=LightOS.img bs=512 count=1 conv=notrunc`

- **bs=512**: `bs`代表块大小（block size）。这里设置为512字节，意味着`dd`在处理数据时会以512字节为单位进行读取和写入操作。这个大小通常与硬盘的扇区大小相匹配，特别是对于MBR（主引导记录）或其他引导扇区来说，512字节是标准的扇区大小。
- **count=1**: `count`指定要复制的块数量。这里设置为1，表示只复制1个块，即512字节的数据从`boot.bin`到`master.img`。这通常用于写入单个引导扇区，因为一个标准的引导扇区大小就是512字节。
- **conv=notrunc**: `conv`（convert的缩写）用于指定转换选项。`notrunc`意味着不截断输出文件，也就是说，`dd`不会在写入数据后截断`master.img`文件。这是重要的，特别是当你只想修改`master.img`中的特定部分而不影响文件其余部分时。如果没有这个选项，`dd`可能会将`master.img`的大小修改为仅包含写入的数据大小。

### 启动 bochs
`bochs -q`可以使用默认配置自动启动，但是首次启动需要手动需改配置，先不使用-q跳过，选择4来保存一份配置，名称必须为bochsrc，否则无法默认指定
修改内容如下：
display_library: x, options="gui_debug"
ata0-master: type=disk, path="LightOS.img", mode=flat
之后就可以顺利通过bochs -q启动界面与debug界面了。

## 一些知识点

* gdb 打印栈：`-exec display/16xw $sp`，16为打印数。
* DWARF：调试信息（debugging information），用于调试，获得调试异常
* CFI也就是控制流完整性，gcc编译成汇编时会自动添加开头部分的伪指令
* 硬盘类型，以及为什么选择IDE启动？

## Makefile

为了自动化编译内核，需要使用make
```
boot.bin: boot.asm
    nasm -f bin boot.asm -o boot.bin
LightOS.img: boot.bin
    yes | bximage -q -hd=16 -mode=create -sectsize=512 -imgmode=flat LightOS.img # 将yes直接输入到bximage命令中，让其自动更新覆盖原来的镜像文件
    dd if=boot.bin of=LightOS.img bs=512 count=1 conv=notrunc
```

# Boot

boot.asm -> loader.asm -> head.asm (ld) kernel

## 硬盘读写

- CHS 模式 / Cylinder / Head / Sector
- LBA 模式 / Logical Block Address

LBA28，总共能访问 128G 的磁盘空间；

硬盘控制端口

| Primary 通道            | Secondary 通道 | in 操作      | out 操作     |
| ----------------------- | -------------- | ------------ | ------------ |
| 0x1F0                   | 0x170          | Data         | Data         |
| 0x1F1                   | 0x171          | Error        | Features     |
| 0x1F2                   | 0x172          | Sector count | Sector count |
| 0x1F3                   | 0x173          | LBA low      | LBA low      |
| 0x1F4                   | 0x174          | LBA mid      | LBA mid      |
| 0x1F5                   | 0x175          | LBA high     | LBA high     |
| 0x1F6                   | 0x176          | Device       | Device       |
| 0x1F7                   | 0x177          | Status       | Command      |

- 0x1F0：16bit 端口，用于读写数据
- 0x1F1：检测前一个指令的错误
- 0x1F2：读写扇区的数量
- 0x1F3：起始扇区的 0 ~ 7 位
- 0x1F4：起始扇区的 8 ~ 15 位
- 0x1F5：起始扇区的 16 ~ 23 位
- 0x1F6:
    - 0 ~ 3：起始扇区的 24 ~ 27 位
    - 4: 0 主盘, 1 从片
    - 6: 0 CHS, 1 LBA
    - 5 ~ 7：固定为1
- 0x1F7: out
    - 0xEC: 识别硬盘
    - 0x20: 读硬盘
    - 0x30: 写硬盘
- 0x1F7: in / 8bit
    - 0 ERR
    - 3 DRQ 数据准备完毕
    - 7 BSY 硬盘繁忙

## 实模式的内存布局

| 起始地址  | 结束地址  | 大小     | 用途               |
| --------- | --------- | -------- | ------------------ |
| `0x000`   | `0x3FF`   | 1KB      | 中断向量表         |
| `0x400`   | `0x4FF`   | 256B     | BIOS 数据区        |
| `0x500`   | `0x7BFF`  | 29.75 KB | 可用区域           |
| `0x7C00`  | `0x7DFF`  | 512B     | MBR 加载区域       |
| `0x7E00`  | `0x9FBFF` | 607.6KB  | 可用区域           |
| `0x9FC00` | `0x9FFFF` | 1KB      | 扩展 BIOS 数据区   |
| `0xA0000` | `0xAFFFF` | 64KB     | 用于彩色显示适配器 |
| `0xB0000` | `0xB7FFF` | 32KB     | 用于黑白显示适配器 |
| `0xB8000` | `0xBFFFF` | 32KB     | 用于文本显示适配器 |
| `0xC0000` | `0xC7FFF` | 32KB     | 显示适配器 BIOS    |
| `0xC8000` | `0xEFFFF` | 160KB    | 映射内存           |
| `0xF0000` | `0xFFFEF` | 64KB-16B | 系统 BIOS          |
| `0xFFFF0` | `0xFFFFF` | 16B      | 系统 BIOS 入口地址 |

## 内存检测

大概意思是说，操作系统在内存检测之前是无法使用内存的，因此需要调用bios中断实现内存检测获取信息，不知道这个“必须检测”是由于硬件限制还是缺乏内存信息的限制。

参考文献 https://wiki.osdev.org/Detecting_Memory_(x86)

BIOS 0x15 子功能号eax=0xe820
核心就是 bios 中断交互，没啥需要注意的，具体手册可查

## 保护模式

### 全局描述符
* 全局描述符表 GDT

  ![](./markdown_img/80386-segment descriptor.jpg)


```c
typedef struct descriptor /* 共 8 个字节 */
{
    unsigned short limit_low;      // 段界限 0 ~ 15 位
    unsigned int base_low : 24;    // 基地址 0 ~ 23 位 16M
    unsigned char type : 4;        // 段类型
    unsigned char segment : 1;     // 1 表示代码段或数据段，0 表示系统段
    unsigned char DPL : 2;         // Descriptor Privilege Level 描述符特权等级 0 ~ 3
    unsigned char present : 1;     // 存在位，1 在内存中，0 在磁盘上
    unsigned char limit_high : 4;  // 段界限 16 ~ 19;
    unsigned char available : 1;   // 该安排的都安排了，送给操作系统吧
    unsigned char long_mode : 1;   // 64 位扩展标志
    unsigned char big : 1;         // 32 位 还是 16 位;
    unsigned char granularity : 1; // 粒度 4KB 或 1B
    unsigned char base_high;       // 基地址 24 ~ 31 位
} __attribute__((packed)) descriptor;
```

> **type segment = 1**
>
> | X | C/E | R/W | A |
>
> A: Accessed 是否被 CPU 访问过
>
> X: 1/代码 0/数据
>
> X = 1：代码段
>
> * C: 是否是依从代码段
> * R: 是否可读
>
> X = 0: 数据段
>
> * E: 0 向上扩展 / 1 向下扩展
>
> * W: 是否可写

### 段选择子

- 只需要一个代码段
- 需要一个或多个数据段 / 栈段 / 数据段
- 加载到段寄存器中 / 校验特权级

```cpp
typedef struct selector
{
    unsigned char RPL : 2; // Request PL 
    unsigned char TI : 1; // 0  全局描述符 1 局部描述符 LDT Local 
    unsigned short index : 13; // 全局描述符表索引
} __attribute__((packed)) selector;
```

- cs / ds / es / ss
- fs / gs

### 开启保护模式

为了向前兼容实模式应用，默认 A20 不开启。兼容性是设计上的重大包袱。最早的8086处理器有20条地址线（A0-A19），能够访问1MB的内存地址空间（2^20 = 1MB），不开启 A20 则会导致超过20位的地址被忽略，并使得地址卷绕，即 0x100000=0x0，这就是为了兼容性。开启 A20 后才能访问超过 1M 的内存。

* 实模式 段地址 * 16 + 段偏移 大于 1M，可通过地址回绕确认开启保护模式
* 0x92 端口第二位置 1 即可开启 A20
* cr0 寄存器 0 位 置为 1 即可开启保护模式

保护模式特性：

- **特权级别（PL）**：开启了多级特权机制，增强了内存和资源的保护。
- **地址寻址方式**：从实模式的段：偏移地址转换为基于GDT/LDT的段选择子和段描述符管理。
- **分页机制（可选）**：在保护模式下，可以启用分页机制，实现更高级的内存管理。

# 进入内核

这个内核不会超过100k，理论上需要寻找内存空间可用地址，实际上bios中断、引导mbr等内容都不重要了，但 BIOS 数据区在系统启动时由 BIOS 初始化，它包含各种硬件和系统状态信息。我先将其256字节拷贝到0x90000位置待用，这个位置基本处于可用内存大块的末尾（`0x7E00`-`0x9FBFF`）。（内核不超过100k即0x19000，之后有用再说）

> #### BIOS 数据区内容
>
> - 0x400 - 0x4FF（256字节）
>
>   ：存储了以下信息：
>
>   - **0x400 - 0x40F**：COM1 - COM4 基地址
>   - **0x410 - 0x417**：LPT1 - LPT3 基地址
>   - **0x418 - 0x41F**：EBDA（扩展 BIOS 数据区）地址
>   - **0x420 - 0x433**：硬盘控制器数据
>   - **0x440 - 0x44F**：硬盘参数表
>   - **0x450 - 0x45F**：键盘缓冲区头和尾指针
>   - **0x460 - 0x46F**：显示模式参数
>   - **0x470 - 0x4FF**：其他系统状态和硬件信息

先将内核在实模式加载到0x10000，为了不覆盖bios中断，后面的loader需要使用他进行内存检测和print的功能，之后，在bios使用完成后使用实模式将0x10000移动到0x0，不能在保护模式的原因是，如果仿照onix在loader中写一个跳转刷新保护模式的jmp，需要重写一个0x90200的gdt，很麻烦也不精美。

拷贝过程需要注意实模式仅仅支持0xffff数量级的读取，因此单次不超过128块扇区，分两次拷贝。

很多指令隐含了ds寄存器，因此移动到0x90000开始的内存后，需要注意修改ds使其指向正确。没办法实模式就是需要偏移才能算出地址，很麻烦。

最后卡在了 jmp 8:0 的位置，lgdt 可能出现了问题，加载不到0x90200偏移的gdt，不知道为什么，而且debug查gdt就会卡死。现象是一旦会跳到0xfffffff0位置，这个应该是bochs的入口，应该是跳到了奇怪的地址导致崩溃重启了。

后来想了想不要当完美主义者，学习原理为主，这种属于汇编和体系结构硬伤，没必要死磕，最终放弃了学linux 0.11的0x90000内存布局的想法，跟着onix走就行了。bug代码保存了一份，放在了`dev_bug_rec/lgdt-0x90200，jmp 8,0 跳转失败.zip`

## ELF

https://refspecs.linuxfoundation.org/elf/elf.pdf

## 整理（目录、makefile、bochs-gdb调试）

新增了boot、kernel、include两个目录，主要是修改makefile中的 src、build等目录，以及在编译kernel时要注意关闭编译器默认的引入，如关闭pie、不引入标准头标准库、-m32指定32位程序等。

### bochs-gdb调试配置

从https://sourceforge.net/projects/bochs/files/bochs/2.8/ 下载bochs-2.8源码，或者github release 2.8中bochs文件夹也可以。

bochs调试有两种，一种自带的debugger，另一种是gdb远程，而两者不共存，因此可以编译两份，放在不同目录，我应该是wsl2直接安装了bochs，导致默认配置了自带的debugger而无法使用gdb远程，因此重新编译一份，参考https://zhuanlan.zhihu.com/p/492780020，并且参考archlinux的pkgbuild安装文件的configure，内容如下

```shell
./configure \
        --prefix=/xxxxxx \
        --without-wx \
        --with-x11 \
        --with-x \
        --with-term \
        --disable-docbook \
        --enable-cpu-level=6 \
        --enable-fpu \
        --enable-3dnow \
        --enable-disasm \
        --enable-long-phy-address \
        --enable-disasm \
        --enable-pcidev \
        --enable-usb \
        --with-sdl \
        --enable-all-optimizations \
        --enable-gdb-stub \
        --with-nogui
        
./configure --prefix=/mnt/e/markdown/OpreatingSystem/LightOS/bochs-2.7/gdb --without-wx --with-x11 --with-x --with-term --disable-docbook --enable-cpu-level=6 --enable-fpu --enable-3dnow --enable-disasm --enable-long-phy-address --enable-disasm --enable-pcidev --enable-usb --with-sdl --enable-all-optimizations --enable-gdb-stub --with-nogui

# 上面的执行后没有Makefile，又参考知乎的写了下面的，重点看了gdb相关，其他的没管，，，

./configure --with-x11 --with-wx --enable-disasm --enable-all-optimizations --enable-readline --enable-long-phy-address --enable-ltdl-install --enable-idle-hack --enable-plugins --enable-a20-pin --enable-x86-64 --enable-cpu-level=6 --enable-large-ramfile --enable-repeat-speedups --enable-fast-function-calls  --enable-handlers-chaining  --enable-trace-linking --enable-configurable-msrs --enable-show-ips  --enable-debugger-gui --enable-iodebug --enable-logging --enable-assert-checks --enable-fpu --enable-vmx=2 --enable-svm --enable-3dnow --enable-alignment-check  --enable-monitor-mwait --enable-avx  --enable-evex --enable-x86-debugger --enable-pci --enable-usb --enable-voodoo -enable-gdb-stub --prefix=/mnt/e/markdown/OpreatingSystem/LightOS/bochs-2.8/bochs-gdb

# 编译
make -j 6

# 其中找不到 x11的头文件
sudo apt install libx11-dev

sudo make install
```

之后一定要注意这里踩坑了，包括视频本身，应该**都是在linux中远程连接开发的**，也就是利用vscode的ssh远程开发功能，所以可以通过本地的 gdb 进行远程调试（类似 gdb +gdbserver实现的，估计是gdbserver 被bochs gdb 集成了）。下面是vscode 中的 launch.json

```json
{
    "version": "0.2.0",
    "configurations": [
      {
        "name": "WSL2 GDB 调试",
        "type": "cppdbg",
        "request": "launch",
        "program": "${workspaceFolder}/build/kernel.bin", //kernel.bin
        "args": [],
        "stopAtEntry": false,
        "cwd": "${workspaceFolder}",
        "environment": [],
        "externalConsole": false,
        "MIMode": "gdb",
        "setupCommands": [
          {
            "description": "Enable pretty-printing for gdb",
            "text": "-enable-pretty-printing",
            "ignoreFailures": true
          }
        ],
        "miDebuggerServerAddress": "172.30.162.247:1234",
        "miDebuggerPath": "/usr/bin/gdb"
      }
    ]
  }
```

但是遇到了两个问题，第一个，bochs终端连接gdb时会疯狂输出地址，从高到低一直到起始位置，基本要输出好几分钟，这期间什么也不能干。

第二个问题，等待输出之后，发现无法运行到断点。

用up的configure重新编译一下gdb，其中根据评论区，重新下载bochs2.7，并且补全了依赖

```sh
sudo apt-get install libncurses5-dev libncursesw5-dev
sudo apt-get install libsdl1.2-dev
```

之后同样的configure就会生成Makefile了

```ini
./configure --prefix=/mnt/e/markdown/OpreatingSystem/LightOS/bochs-2.7/bochs-gdb --without-wx --with-x11 --with-x --with-term --disable-docbook --enable-cpu-level=6 --enable-fpu --enable-3dnow --enable-disasm --enable-long-phy-address --enable-disasm --enable-pcidev --enable-usb --with-sdl --enable-all-optimizations --enable-gdb-stub --with-nogui

# 后续make报错，缺少gui，‘XRRQueryExtension’ was not declared
sudo apt-get install libxrandr-dev
# 无效，尝试gpt的禁用 XRandR 的方案

./configure --prefix=/mnt/e/markdown/OpreatingSystem/LightOS/bochs-2.7/bochs-gdb --without-wx --with-x11 --with-x --with-term --disable-docbook --enable-cpu-level=6 --enable-fpu --enable-3dnow --enable-disasm --enable-long-phy-address --enable-disasm --enable-pcidev --enable-usb --with-sdl --enable-all-optimizations --enable-gdb-stub --with-nogui --disable-xrandr
 
# 不过configure识别不了 --enable-disasm --disable-xrandr，但是这次 make 居然就成了？之后重试上面的也可以了。玄学。
# 问题很多，运行又出现了 >>PANIC<< Plugin 'gameport' not found的问题，configure + --enable-gameport 之后又出现了 >>PANIC<< Plugin 'iodebug' not found的问题，感觉不对劲，从头思考一下。

# 尝试 上面知乎给的 configure + bochs 2.6.11
./configure --with-x11 --with-wx --enable-disasm --enable-all-optimizations --enable-readline --enable-long-phy-address --enable-ltdl-install --enable-idle-hack --enable-plugins --enable-a20-pin --enable-x86-64 --enable-cpu-level=6 --enable-large-ramfile --enable-repeat-speedups --enable-fast-function-calls  --enable-handlers-chaining  --enable-trace-linking --enable-configurable-msrs --enable-show-ips  --enable-debugger-gui --enable-iodebug --enable-logging --enable-assert-checks --enable-fpu --enable-vmx=2 --enable-svm --enable-3dnow --enable-alignment-check  --enable-monitor-mwait --enable-avx  --enable-evex --enable-x86-debugger --enable-pci --enable-usb --enable-voodoo -enable-gdb-stub --prefix=/mnt/e/markdown/OpreatingSystem/LightOS/bochs-2.6.11/bochs-gdb

./configure --with-x11 --with-wx --enable-disasm --enable-all-optimizations --enable-readline --enable-long-phy-address --enable-ltdl-install --enable-idle-hack --enable-plugins --enable-a20-pin --enable-x86-64 --enable-cpu-level=6 --enable-large-ramfile --enable-repeat-speedups --enable-fast-function-calls  --enable-handlers-chaining  --enable-trace-linking --enable-configurable-msrs --enable-show-ips  --enable-debugger-gui --enable-iodebug --enable-logging --enable-assert-checks --enable-fpu --enable-vmx=2 --enable-svm --enable-3dnow --enable-alignment-check  --enable-monitor-mwait --enable-avx  --enable-evex --enable-x86-debugger --enable-pci --enable-usb --enable-voodoo -enable-gdb-stub --prefix=/mnt/e/markdown/OpreatingSystem/LightOS/bochs-2.8/bochs-gdb

还是报错，有的无法编译，有的编译好了没法运行，也有的运行之后报错gameport插件
```

### 失败

配不好了，用2.8测了，同样的configure编译出来的bochs不一样，用bochs生成的配置文件不匹配，后编译的没有debug-stub，如果用后来编译出来的bochs强行运行之前的配置会报缺少iodebug插件，看来是configure编译出来的bochs不一样。

### 新的环境配置

问了lyq之后考虑放弃bochs，采用vmware+ubuntu+qemu的开发测试环境。首先ubuntu开启远程ssh。注，vscode将在2025取消对老内核的远程连接支持，所以ubuntu需要 >= 20.04

```bash
sudo apt update
sudo apt install openssh-server
sudo gedit /etc/ssh/sshd_config
# 将PermitRootLogin prohibit-password那一行修改为PermitRootLogin yes，去掉前面的#号
# 将port 22前面的#去掉3

sudo systemctl restart ssh
```

ubuntu 安装 qemu

```bash
sudo apt install qemu
# 16.04会自动安装一些工具，如qemu-utils下的qemu-image
# 22.04不安装，手动安装工具
sudo apt-get install qemu-system-mips
sudo apt-get install qemu-user
sudo apt-get install qemu-utils
```

共享文件夹：https://blog.csdn.net/weixin_51111267/article/details/132343320

```bash
#首先在vmware设置中添加共享文件夹

# 末尾添加映射
chmod 777 /etc/fstab
.host:/ /mnt/hgfs fuse.vmhgfs-fuse allow_other,uid=0,gid=0,umask=022 0 0

# 链接
ln -s /mnt/hgfs/LightOS ~/Desktop
```

---

后面干了挺多东西，比如添加git，创建vmware虚拟机，这些对应p16。其中vscode 的 git ui 中 commit 会报错没有username 和 email，命令行就好了。还有一个坑就是，vmware创建虚拟机时候的最后一步指定了已经存在的镜像文件后，需要“转换镜像格式”，否则会boot error，评论区说可能是55aa的问题。

## 端口输入输出

端口就是外部设备的内部寄存器编号。显示相关的端口：

```cpp
#define CRT_ADDR_REG 0X3D4
#define CRT_DATA_REG 0X3D5
#define CRT_CURSOR_H 0XE
#define CRT_CURSOR_L 0XF
# 一个是地址寄存器，一个是数据寄存器
# 读取：首先将需要读取的值（CURSOR_H/L）写入CRT_ADDR_REG也就是显示端口，之后读端口的数据寄存器（记得延迟一会再返回）
void read_cursor(){
    outb(CRT_ADDR_REG, CRT_CURSOR_H);
    u16 pos = inb(CRT_DATA_REG) <<8;
    outb(CRT_ADDR_REG, CRT_CURSOR_L);
    pos |= inb(CRT_DATA_REG);
}
#写入同理
void write_cursor(){
	outb(CRT_ADDR_REG, CRT_CURSOR_H);
    outb(CRT_DATA_REG, 0);
    outb(CRT_ADDR_REG, CRT_CURSOR_L);
    outb(CRT_DATA_REG, 0);
}
```

## 字符串处理

https://en.cppreference.com/w/c/string/byte

## 显卡驱动

- CGA (Color Graphics Adapter)
    - 图形模式
        - 160 * 100
        - 320 * 240
        - 640 * 200
    - 文本模式
        - 40 * 25
        - 80 * 25
- EGA (Enhanced Graphics Adapter)
- MCGA (Multi Color Graphics Array)

### CRTC (Cathode Ray Tube Controller)

CGA 使用的 MC6845 芯片；

- CRT 地址寄存器 0x3D4
- CRT 数据寄存器 0x3D5
- CRT 光标位置 - 高位 0xE
- CRT 光标位置 - 低位 0xF
- CRT 显示开始位置 - 高位 0xC
- CRT 显示开始位置 - 低位 0xD

控制字符参考 onix 笔记 022 基础显卡驱动，这里不记录了。

写终端驱动挺有意思的

## 可变参数+printk

原理是通过宏来自动获取栈上参数，实际上很好实现，我认为本质上是编译器默许了调用者的任意参数压栈。

C 中实现于 stdarg.h，linux中也有实现。

```c
typedef char *va_list;

#define stack_size(t) (sizeof(t) <= sizeof(char *) ? sizeof(char *) : sizeof(t))
#define va_start(ap, v) (ap = (va_list)&v + sizeof(char *))
#define va_arg(ap, t) (*(t *)((ap += stack_size(t)) - stack_size(t)))
#define va_end(ap) (ap = (va_list)0)
```

此外，printf 的 format 中也有函数参数数量的标记，有多少个百分号，就有多少个参数。

### 格式指示串的形式

> `%[flags][width][.prec][h|l|L][type]`


- `%`：格式引入字符
- `flags`：可选的标志字符序列
- `width`：可选的宽度指示符
- `.prec`：可选的精度指示符
- `h|l|L`：可选的长度修饰符
- `type`：转换类型

### flags

flags 控制输出对齐方式、数值符号、小数点、尾零、二进制、八进制或十六进制等，具体格式如下：

- `-`：左对齐，默认为右对齐
- `+`：输出 + 号
- ` `：如果带符号的转换不以符号开头则添加空格，如果存在 `+` 则忽略
- `#`：特殊转换：
    - 八进制，转换后字符串首位必须是 `0`
    - 十六进制，转换后必须以 `0x` 或 `0X` 开头
- `0`：使用 `0` 代替空格

直接用现成的库实现的vsprintf和sprintf，包装一层作为printk即可。

## 断言

一个宏

```c
#define assert(exp)     \
    if (exp);           \
    else                \
        assertion_failure(#exp, __FILE__, __BASE_FILE__, __LINE__)
```



## gdt 重载

在c中规划gdt数组并lgdt。

## 任务调度

视频里的是假的任务调度，没利用中断也没有内存分配而是手动指定两个进程的起始地址，类似协程（pcb与内核栈共用4k，与linux设计一致）。因此暂时跳过了。

## 中断

终于来了，操作系统最重要的功能之一。首先明确，所有的中断都是通过IDT实现的，其中前32号为 IA-32 定义的异常处理，也就是陷阱门，之后是硬件中断，通过中断控制器（PIC或APIC）映射到33开始的位置，可以通过修改中断控制器来修改映射。之后的 IDT 可以自行设定，比如 int 0x80 就是对应着 IDT 128 号。

> 内中断：
>
> * 软中断：系统调用
> * 异常
>   * 除零
>   * 指令错误
>   * 缺页错误
>
> 外中断：
>
> * 时钟
> * 键盘
> * 硬盘
>   * 同步端口 IO
>   * 异步端口 IO
>   * DMA

内中断还分为：任务门、中断门、陷阱门，但是由于任务门的效率和灵活度比较低，因此一般不使用、而x64的处理器直接摒弃了任务门。系统调用等都是通过中断门来进行的。

### 中断门

中断门自动置IF 0，防止中断时发生其他中断。

INT要看是什么模式，还有是否更改CPL，行为比较复杂，反正最后压入堆栈的三个是EFLAGS,CS和下一条指令的EIP。

1. eip
2. cs 
3. eflags

### 异常

异常分为三种：

* 故障 Fault：可被修复的一种，最轻的一种异常，比如包括除零错误，进程可以被捕获然后被终止或者其他异常处理等。
* 陷阱 Trap：常用于调试。
* 终止 Abort：最严重的异常类型，一旦出现将无法修复，程序无法继续运行。

有些异常会在栈中压入错误码，有些则没有。

### 异常列表（IA-32 定义了32种异常类型）

| 编号              | 名称           | 类型      | 助记符  | 错误码    |
| ----------------- | -------------- | --------- | ------- | --------- |
| 0 (0x0)           | 除零错误       | 故障      | #DE     | 无        |
| 1 (0x1)           | 调试           | 故障/陷阱 | #DB     | 无        |
| 2 (0x2)           | 不可屏蔽中断   | 中断      | -       | 无        |
| 3 (0x3)           | 断点           | 陷阱      | #BP     | 无        |
| 4 (0x4)           | 溢出           | 陷阱      | #OF     | 无        |
| 5 (0x5)           | 越界           | 故障      | #BR     | 无        |
| 6 (0x6)           | 指令无效       | 故障      | #UD     | 无        |
| 7 (0x7)           | 设备不可用     | 故障      | #NM     | 无        |
| 8 (0x8)           | 双重错误       | 终止      | #DF     | 有 (Zero) |
| 9 (0x9)           | 协处理器段超限 | 故障      | -       | 无        |
| 10 (0xA)          | 无效任务状态段 | 故障      | #TS     | 有        |
| 11 (0xB)          | 段无效         | 故障      | #NP     | 有        |
| 12 (0xC)          | 栈段错误       | 故障      | #SS     | 有        |
| 13 (0xD)          | 一般性保护异常 | 故障      | #GP     | 有        |
| 14 (0xE)          | 缺页错误       | 故障      | #PF     | 有        |
| 15 (0xF)          | 保留           | -         | -       | 无        |
| 16 (0x10)         | 浮点异常       | 故障      | #MF     | 无        |
| 17 (0x11)         | 对齐检测       | 故障      | #AC     | 有        |
| 18 (0x12)         | 机器检测       | 终止      | #MC     | 无        |
| 19 (0x13)         | SIMD 浮点异常  | 故障      | #XM/#XF | 无        |
| 20 (0x14)         | 虚拟化异常     | 故障      | #VE     | 无        |
| 21 (0x15)         | 控制保护异常   | 故障      | #CP     | 有        |
| 22-31 (0x16-0x1f) | 保留           | -         | -       | 无        |

说明：

0. 当进行除以零的操作时产生
1. 当进行程序单步跟踪调试时，设置了标志寄存器 eflags 的 T 标志时产生这个中断
2. 由不可屏蔽中断 NMI 产生
3. 由断点指令 int3 产生，与 debug 处理相同
4. eflags 的溢出标志 OF 引起
5. 寻址到有效地址以外时引起
6. CPU 执行时发现一个无效的指令操作码
7. 设备不存在，指协处理器，在两种情况下会产生该中断：
    1. CPU 遇到一个转意指令并且 EM 置位时，在这种情况下处理程序应该模拟导致异常的指令
    2. MP 和 TS 都在置位状态时，CPU 遇到 WAIT 或一个转移指令。在这种情况下，处理程序在必要时应该更新协处理器的状态
8. 双故障出错
9.  协处理器段超出，只有 386 会产生此异常
10. CPU 切换时发觉 TSS 无效
11. 描述符所指的段不存在
12. 堆栈段不存在或寻址堆栈段越界
13. 没有符合保护机制（特权级）的操作引起
14. 页不在内存或不存在
15. 保留
16. 协处理器发出的出错信号引起
17. 对齐检测只在 CPL 3 执行，于 486 引入
18. 与模型相关，于奔腾处理器引入
19. 与浮点操作相关，于奔腾 3 引入
20. 只在可以设置 EPT - violation 的处理器上产生
21. ret, iret 等指令可能会产生该异常

### 中断压入地址的区别

触发中断时，cpu压入返回地址有两种情况，一种是压入本身这行，可以在中断返回后重新执行，另一种则是下一行的地址，返回后继续执行。

中断如int 0x80的iret会继续执行下一行，而trap中的缺页异常，会重复执行出现错误的那一条代码，其他类型的中断也不一样，比如无效操作码、除零错误、一般保护异常：通常会终止进程或抛出错误，具体行为取决于异常处理逻辑，而单步调试用的陷阱，则在返回后继续执行下一行。

GPT 给总结了一下

```markdown
在x86架构中，不同类型的异常和中断在发生时会将不同的地址压入堆栈。具体来说，有些异常会压入当前指令的地址，而有些则会压入下一条指令的地址。下面是对常见中断和异常在IDT中的行为的总结：
# 压入当前指令地址的异常（重新执行导致异常的指令）
这些异常通常是“故障（Faults）”，在处理完异常后，会重新执行导致异常的指令：
    除零错误（#DE, Divide Error）：IDT 0
    调试异常（#DB, Debug Exception）：IDT 1
    无效操作码（#UD, Invalid Opcode）：IDT 6
    设备不可用（#NM, Device Not Available）：IDT 7
    双重故障（#DF, Double Fault）：IDT 8
    协处理器段溢出（#MF, Coprocessor Segment Overrun）：IDT 9
    无效TSS（#TS, Invalid TSS）：IDT 10
    段不存在（#NP, Segment Not Present）：IDT 11
    栈段错误（#SS, Stack Segment Fault）：IDT 12
    一般保护错误（#GP, General Protection Fault）：IDT 13
    页错误（#PF, Page Fault）：IDT 14
# 压入下一条指令地址的异常和中断（继续执行后续指令）
这些通常是“陷阱（Traps）”和一些特定的中断，处理完异常后，继续执行异常发生后的下一条指令：
    断点（#BP, Breakpoint）：IDT 3
    溢出（#OF, Overflow）：IDT 4
    单步调试（Debug Exception, 单步）：这是一种特殊情况，一般在调试模式下使用，处理完成后继续执行下一条指令。
# 硬件中断（通常压入下一条指令地址）
硬件中断一般会压入中断发生时的下一条指令的地址，因为它们通常发生在指令执行之间，不影响当前指令的完成：
    例如，IRQ0（时钟中断）：IDT 32
    IRQ1（键盘中断）：IDT 33
    其他硬件中断（根据系统设置在IDT中的位置）。
# 特殊情况
双重故障（#DF, Double Fault）：IDT 8。这种情况比较特殊，通常无法恢复，会导致系统崩溃。
机器检查（#MC, Machine Check）：IDT 18。具体行为可能依赖于具体的硬件实现。
# 总结
压入当前指令地址（重新执行导致异常的指令）：故障（Faults），如除零错误、无效操作码、页错误等。
压入下一条指令地址（继续执行后续指令）：陷阱（Traps），如断点、溢出，以及大多数硬件中断。
```

### 外中断

### eflags

![eflags](.\markdown_img\eflags.png)

8086中采用了两片8259a可编程控制芯片，每片管理8个中断源，两片级联可以控制64个中断向量，但是 PC/AT 系列兼容机最多能够管理15个中断向量。主片端口0x20，从片0xa0。具体原理可参考onix 032 外中断原理，真的很有意思，这才是计算机科学。

![8259a](.\markdown_img\8259a.png)

**sti 和 cli 都是针对外中断的，内中断和异常都是 cpu 片内事件，不受 eflags 中的 if 位控制。**

### 中断上下文

![image-20240602195457164](.\markdown_img\interrupt_context.png)

## 时钟

多与硬件相关。

JIFFY：1000 / HZ，个人理解就是精度，也就是时间中断间隔，比如 HZ = 100 的情况下，精度也就是一个 slice 是 10ms，这也是时间的最小分辨率了。

设置时要注意，CLOCK_COUNTER 是 4 字节，超过范围可能会被截断导致获得了意外的 jiffy，而且 HZ 太高会导致中断太频繁处理不了直接死机，测试大概 1000 就会与正常时间有一点差异了，10000就会卡，再往上就死机了。

```c
#define HZ 100
#define OSCILLATOR 1193182
#define CLOCK_COUNTER (OSCILLATOR / HZ)
```

## 蜂鸣器

qemu中的音频驱动

* **ALSA **：Advanced Linux Sound Architecture
* coreaudio
* dsound
* oss
* PulseAudio
* SDL
* spice
* wav

qemu失败，不知道什么原因，vmware中能够正常发声。考虑到不是什么重要功能，qemu就不调试了，跳过。

第二天再看，好像是makefile少写了一个 \ 导致配置没加载进去，现在正常了。

## 时钟

没什么好说的，就是从cmos读取时间戳bcd转时间就ok。

这里注意与中断相关的要放在 interrupt_init 后面，因为初始化将所有中断都关了。

# 内存管理

![image-20240604113454497](.\markdown_img\image-20240604113454497.png)

## ards

先获取loader中利用bios中断得到的内存检测结果结构体 ards_t，通过遍历找到最大的内存空间作为有效内存。

## 内存管理模式与地址概念

> ### 平坦模式（Flat Mode）
>
> #### 设计原理：
>
> 平坦模式是一种内存模型，其中所有内存地址直接映射到物理地址空间，而不进行任何分段或分页转换。在这种模式下，逻辑地址等同于线性地址，也等同于物理地址。
>
> #### 与GDT的关系：
>
> 尽管平坦模式不实际使用分段，但仍然需要设置一个全局描述符表（GDT）。在平坦模式中，GDT中的段描述符被设置为覆盖整个地址空间，并且段基址（Base）为0，段限长（Limit）设置为最大值（4GB）。这样，所有段寄存器（如CS、DS、SS等）的基址都为0，段限长为最大值，从而实现平坦地址空间。
>
> ### 分段模式（Segmentation）
>
> 分段模式通过将内存分割成多个段，每个段都有自己的基址和限长。逻辑地址由段选择子和段内偏移组成。段选择子指向GDT中的段描述符，通过段描述符可以找到段的基址和限长，然后将段内偏移加到基址上形成线性地址。
>
> ### 分页模式（Paging）
>
> 分页模式将内存划分为固定大小的页（通常为4KB），并使用页表（Page Table）将线性地址映射到物理地址。线性地址分为页目录项、页表项和页内偏移三部分，通过查找页目录和页表，将线性地址转换为物理地址。
>
> ### 地址概念
>
> 1. **逻辑地址（Logical Address）**：
>    - 又称虚拟地址（Virtual Address），由段选择子和段内偏移组成。逻辑地址需要经过分段机制转换为线性地址。
> 2. **线性地址（Linear Address）**：
>    - 经过分段机制转换得到的地址，在分页模式下，它进一步通过页表转换为物理地址。在平坦模式下，线性地址直接等同于物理地址。
> 3. **物理地址（Physical Address）**：
>    - 内存芯片中的实际地址，由内存控制器直接访问。在平坦模式下，线性地址直接映射为物理地址；在分页模式下，线性地址通过页表转换为物理地址。
> 4. **虚拟地址（Virtual Address）**：
>    - 在某些上下文中，虚拟地址可以泛指逻辑地址，但在分页模式下，虚拟地址通常特指经过分页机制映射的用户程序地址空间。
>
> ### 各模式下的地址转换关系
>
> - **平坦模式**： 逻辑地址 = 线性地址 = 物理地址
> - **分段模式**： 逻辑地址 = (段选择子, 段内偏移) → 线性地址
> - **分页模式**： 线性地址 = (页目录项, 页表项, 页内偏移) → 物理地址
>
> ### 具体示例
>
> #### 分段模式下地址转换：
>
> 假设段选择子指向的段描述符基址为0x1000，限长为0xFFFF，段内偏移为0x2000，那么逻辑地址0x2000将被转换为线性地址0x3000。
>
> #### 分页模式下地址转换：
>
> 假设页目录项指向的页表基址为0x4000，页表项指向的物理页基址为0x8000，页内偏移为0x100，那么线性地址0x1200100将被转换为物理地址0x8100。
>
> 通过理解这些地址转换机制，可以更好地理解CPU内存管理和操作系统的地址空间管理。

## 页表

![image-20240605222254688](.\markdown_img\页表.png)

已经学了无数遍了，应该大概可能记得很熟了吧()。

**实现的时候注意一点**，页表中的20位index是cpu寻址用的，而内容是>>12位的 IDX(addr)，所以 index 就可以找遍 4G了，页表可以放在任何位置，这一点困扰了我好一会。

设计过程中发现，页表放在前面确实不错，能够实现内核中物理地址=线性地址的操作，方便后续修改页表。但是前面只留了 0x10000 也就是 64k 作为页目录表、页表以及内核栈，所以需要判断分配页表是否超过，超过的话可能要修改页表位置了。

```c
in void memory_init(u32 magic, u32 addr) of memory.c
	// 前 0x10000 给了页表和内核栈，最多64k
    // 给内核栈4k吧，剩余60k能放15个页表
    // 除去开头4个页目录表，初始化的内存不能超过11个页表
    // 映射的最大内存为 (11*1024*4096)/1024/1024 = 44M
    // 当前有效内存是 0x1ee0000 + 1M = 31.875 M
    assert (total_pages / 1024 <= 11);
```

**但是，后来实现的时候才想明白，页表不能更不应该映射所有可用的内存空间，后面的空间是给程序的，并触发 PF 后动态映射来占用和使用的，所以 Linux 0.11 应该是只映射了 1M 给内核，剩下的应该是没动，都给程序了**。

onix最开始选择映射了 8M 给内核，也就是两个页表，不过后来的代码改成了四个页也就是16M，这里先参考它的8M来实现吧。

修改完页目录和页表之后，需要刷新快表TLB

```assembly
# 刷新所有页目录和页表
mov cr3, eax
# 效率高
invlpg
```

![highlight](.\markdown_img\highlight.png)

**值得注意的是，onix选择了0x1000做页目录表，0x2000 0x3000做两个页表，后面的0x4000做内核虚拟内存位图缓存**，参考下图。而在我的LightOS中，我完整的保留了从0开始的四个页目录，并且参考onix做了两个页表给内核，所以布局应该是

0x0000-0x3fff 四个页目录表
0x4000-0x5fff 两个页表（专门为内核提供 8M 映射）
0x6000-0x6fff 内核虚拟内存位图缓存

## 后续更新

`用户内存映射管理`，重新梳理了一下页表，发现有误：

linux0.11的内核部分是一个pd+4个pt，总共映射了16M给内核，与onix实现一致。我记成了4个pd，并且也没仔细计算。

* PD永远都是1个，这是根据分页的地址分割决定的，10/10/12的分割刚好让一个页的PD可以覆盖10位的索引！
* 64位也一样，四级分页9/9/9/9/12（48bits），加载到cr3的PD永远只有一个页，不存在跨页的情况！
* 后续为了给内核分配更多空间（1G），需要增加PDE（PT）到 1G/4M = 256 个项。
* 也可以直接参照这两者的实现，映射16M给内核，也够用了。

修复了mapping_init的相关代码。

## 简单总结

这地方有点乱，总结梳理一下：

> ## 1. page_mapping
>
> 由于开启分页后必须通过二级paging机制才能找到物理地址，并且由于内核管理页面分配时还是按照物理地址的方式进行的，因此内核里必须有一个分页能够将线性地址等价映射到物理地址上，方便后续内核直接使用“物理地址”（其实都经过了分页的映射）进行管理。
>
> 在 LightOS 中，我映射了pd页目录表的前两项pde到了相应pt页表，并且采取了物理地址=线性地址的映射方式，因此实际上内核目前能够管理的物理地址就只有8M了，但是着并不妨碍初始化后面的mem_map为完整的内存，因为后面的地址都是通过虚拟地址的方式动态分配页的，因此需要分配时自然会为其绑定页表，而目前也不需要真实使用后面的地址空间，因此符合设计逻辑。
>
> ## 2. mem_map
>
> mem_map，物理内存数组，是用来统计页的引用计数的，其位于0x100000也就是1M的位置，其记录了全部的可用内存的使用情况，无论是内核内存还是用户内存，计算其所占页数的代码如下所示：
>
> ```c++
> total_pages = IDX(memory_size) + IDX(MEMORY_BASE);
> mem_map_pages = div_round_up(total_pages, PAGE_SIZE);
> ```
>
> 而在memory_map_init初始化过程中，会将mem_map中，物理内存数组（包括）及以前的所有内存空间都设置为已经使用（USED），也就是说，从start_page之前的物理内存都不可以再次分配了，被内核通过物理空间的形式使用掉了，而start_page及后面的空间则可以供虚拟内存动态分配给内核或用户空间地址。
>
> 分配一个物理页（get_page）的时候，从start_page开始查找第一个非USED的页面。
>
> 而虚拟内存位图缓存是为了快速检查页是否占用的，是用于分配虚拟内存时查找更加方便。

# 创建内核线程

## 硬件任务切换

想了一下，发现目前的Onix进程切换采用的不是硬件进程切换，而是手动jmp到eip的方式，而且没有做task的状态保存，也就是说不可恢复。而硬件进程切换 Hardware Context（Task） Switching需要实现 TSS在GDT中的部署。

```c#
// Linux 0.11 sched.h
/*
 * Entry into gdt where to find first TSS. 0-nul, 1-cs, 2-ds, 3-syscall
 * 4-TSS0, 5-LDT0, 6-TSS1 etc ...
 */
#define FIRST_TSS_ENTRY 4
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY+1)
#define _TSS(n) ((((unsigned long) n)<<4)+(FIRST_TSS_ENTRY<<3))
#define _LDT(n) ((((unsigned long) n)<<4)+(FIRST_LDT_ENTRY<<3))

/*
 *	switch_to(n) should switch tasks to task nr n, first
 * checking that n isn't the current task, in which case it does nothing.
 * This also clears the TS-flag if the task we switched to has used
 * tha math co-processor latest.
 */
#define switch_to(n) {\
struct {long a,b;} __tmp; \
__asm__("cmpl %%ecx,_current\n\t" \
	"je 1f\n\t" \
	"movw %%dx,%1\n\t" \
	"xchgl %%ecx,_current\n\t" \
	"ljmp %0\n\t" \
	"cmpl %%ecx,_last_task_used_math\n\t" \
	"jne 1f\n\t" \
	"clts\n" \
	"1:" \
	::"m" (*&__tmp.a),"m" (*&__tmp.b), \
	"d" (_TSS(n)),"c" ((long) task[n])); \
}
```

> 在Linux 0.11中，全局描述符表（GDT）包含了操作系统需要的所有段描述符，包括代码段、数据段、任务状态段（TSS）和局部描述符表（LDT）的描述符。我们先看一下Linux 0.11中的GDT布局，然后讨论TSS是否必须放在GDT中才能供CPU进行硬件切换。
>
> ### GDT布局
>
> Linux 0.11中的GDT布局大致如下：
>
> 1. **空描述符**（索引0）
> 2. **内核代码段**（索引1）
> 3. **内核数据段**（索引2）
> 4. **系统调用段**（索引3）
> 5. **任务0的TSS描述符**（索引4）
> 6. **任务0的LDT描述符**（索引5）
> 7. **任务1的TSS描述符**（索引6）
> 8. **任务1的LDT描述符**（索引7）
> 9. **更多任务的TSS和LDT描述符**...
>
> ### TSS在GDT中的重要性
>
> 在x86架构中，硬件任务切换需要TSS描述符来保存和恢复任务状态。TSS描述符必须放在GDT中，以便CPU可以访问它们。这是因为硬件任务切换机制依赖于GDT中的TSS描述符来获取任务状态段的位置和属性。
>
> ### 硬件任务切换
>
> 硬件任务切换通过以下步骤进行：
>
> 1. **任务注册器（Task Register, TR）**：
>    - TR包含当前任务的TSS选择子，该选择子指向GDT中的TSS描述符。
>    - 使用`ltr`指令加载新的TSS选择子到TR，告诉CPU当前任务的TSS位置。
> 2. **TSS描述符**：
>    - TSS描述符包含TSS的基地址和限制，描述了TSS段的位置和大小。
> 3. **切换任务**：
>    - 当使用`ljmp`、`call`或`int`指令切换到新的任务时，CPU会根据新的TSS选择子在GDT中查找TSS描述符，加载新的TSS，并更新TR以指向新的TSS

## 软件任务切换

最后还是手动tss切换了，硬件没看手册，写gdt也挺麻烦。

遇到了几个问题

* 第一个是中断后报PF，实际上PF由于首页没有映射，因此是个空指针错误，检查发现current没初始化，解决。

* 进程切换一次后就没有时钟中断了，这个是因为swich to 实际上是从中断里跳转走的，不会再回中断了，因此需要将send_eoi提前发送

  >  // 发送中断处理结束（允许下一次中断到来，否则切进程就没中断了）
  >
  >   send_eoi(vector); 
  
* **由于没有做tss的详细初始化，所以load_state会触发PF（为什么是PF呢），后面写完fork大概就没事了，先把 save 和 load 注释掉了。**

还有一个很奇怪的问题，运行一段时间后系统好像自动重启，从头运行并且没有任何提示，目前不知道原因。

通过下一集 P47 的内容得知，Onix中经常会出现全屏红色的bug，原因是中断发生在了显卡驱动的输出过程中，导致输出了错误的内容。我估计间歇性重启也是因为中断的原因，驱动都不是原子性的。至于解决方案，Onix中暂时禁用中断，因为还没有完成锁相关机制，这部分内容后面补上。

**但是后面测试发现固定位置一定会重启，且并不是显卡驱动的问题，是直接重新运行了，这个目前没有问题的思路。**

**还有如果让调度输出很多内容，那么运行一段时间也会触发 PF，为什么？切换进程不干净吗，爆栈了？**

非常神奇，后面实现了系统调用发现一旦输出多了就会疯狂“重启”，不一定是重启但是有重新启动的现象，并且重启后随机时间发生 PF。正常不切换进程就没有问题，还是进程的问题，等后面实现了fork之后再重新理一遍吧。

### 新进度

https://wiki.osdev.org/Context_Switching

根据OSDEV的描述，现代的操作系统出于性能和可移植性的原因，都会采用软件上下文切换的方式。

**自动重启的原因找到了，因为load的过程出了问题，导致esp和ebp在切换上下文时候被赋值为0，一旦进入新进程函数需要分配局部变量时，esp已经为0，再减就溢出到0xFFFFFFFF，估计是qemu检测到了异常，所以qemu自动重启了。**

此外就是，系统初始化的内核栈与新分配的两个进程不一致，但是是通过进程调用来开始执行第一个进程的，因此导致目前的栈与进程A并不相符，所以只能暂时性的将save删掉了，目前没有办法从yield或者抢占处继续执行该进程。

此外，不可以通过局部变量传递tss，因为一旦恢复程序栈，则当前局部变量丢失，因此需要通过current来保证获取正确的tss。

第二天发现系统的sleep(1000)并不是一秒，而是将近两秒，查了半天没有头绪，猜测可能是虚拟机太卡了就重启了一下，果然恢复了。

# 系统调用

之前已经实现了一部分，先是设置 IDT 表的 0x80 为 syscall，之后在 syscall 中判断 nr_syscall 没超过范围（这里因为C中的宏无法在汇编中extern，所以重复定义了一个u32，不太精美），之后转交给系统调用来处理，结束后返回 eax 作为返回值。

需要注意一点，IDT 中设置的是 gate->DPL = 3; 也就是用户态的中断，在内核中也可以顺利触发，硬件设计中，操作系统权限是向下兼容的。

int 80h 是32位x86的系统调用方式。同样通过ax传递调用号，参数传递顺序是：ebx，ecx，edx，esi，edi

注：intel体系的系统调用限制最多六个参数，没有任何一个参数是通过栈传递的。系统调用的返回结果存放在ax寄存器中，且只有整型和内存型可以传递给内核。

# 链表与任务阻塞

在内核中链表的实现方法都差不多，都是作为一个字段被其他类型引用进去，从而提供链表功能。

至于任务阻塞，linux0.11 的实现是，在任务的函数中设置了一个局部变量 tmp，用其链接current作为一个栈上链表（大致思路）。在 ONIX 中，在 pcb 中加入了链表数据结构，使得pcb可以连成链。

**任务阻塞不是系统调用，没有程序会主动请求阻塞，因此不应作为系统调用实现，而是task的一个功能函数**

# IDLE Task

Process 0，但是没法测试，实际上实现也比较容易，就是不停的调用schedule就行，也跳过。

视频中的idle比较抽象，居然用到了hlt，等待外部中断？clock的时钟中断可以触发程序的调度

# sleep睡眠唤醒

int 80h 是32位x86的系统调用方式。同样通过ax传递调用号，参数传递顺序是：***ebx，ecx，edx，esi，edi***

休眠调用sleep时将该进程 **插入排序** 添加到 sleep_list 链表中，将预计唤醒时间保存在 task->jiffies 中，并在每次 clock 中断时触发链表的检查，若有唤醒程序，则将其状态置 READY 即可。

但是有一个bug，如果基于jiffies进行sleep判断，若一个进程刚刚从被阻塞状态唤醒，就立即执行sleep，则无法触发一次时钟中断更新jiffies，导致jiffies计算出错，后面的sleep不准确，我认为应该在 schedule() 中，切换任务前，更新jiffies。

```c
task_list[n]->jiffies = jiffies;
```

成功修复！

# mutex&signel 互斥与信号量

尚未考虑进程饿死的情况，等待队列也设置为了FIFO。

while的设计非常艺术，减少了关闭中断的上下文长度，见注释

```c
void mutex_lock(mutex_t* mutex){
    // 个人认为这里不需要关中断，因为一旦互斥量释放，只会选择一个等待进程进行唤醒
    // 此时一定可以 inc 获得该互斥量，而多线程情况（新任务未阻塞且同时争抢互斥量）关中断也无效
    // 唯一一种情况是， **判断 - 获取锁** 的过程中被中断的情况，此时调度到其他程序就会出现同时有两个程序操作临界区的情况发生，但是概率较小。
    task_t* current = get_current();
    
    while(mutex->value){
        task_block(current, &mutex->waiters, TASK_BLOCKED);
    }
    mutex->value++; //inc 原子操作
}

void mutex_unlock(mutex_t* mutex){
    // 解锁也不需要关中断，一种情况：mutex--后瞬间被中断，新任务又恰好占用mutex，
    // 调度时阻塞任务被恢复会再次判断mutex是否被占用，并再次进入阻塞。
    task_t* current = get_current();
    
    mutex->value--;
    task_unblock(&mutex->waiters);
}
```

此外需要修复console的临界区操作，若在操作console打印时候发生中断，且另一个程序也尝试写console，则可能会导致显示异常，因此需要通过mutex设置临界区。

但是与下面的自旋锁描述一致，若在判断和获取之间发生了中断，就会导致两个进程获取了同一个互斥资源，所以 **判断 - 获取锁** 必须是原子操作，更新代码如下：

```c
void mutex_lock(mutex_t* mutex) {
    task_t* current = get_current();
    int expected = 0;
    int new_value = 1;

    // 若想不关中断，则必须让“检查+获取锁”是原子操作。
    // 否则若中断于二者中间，此时调度到其他程序就会出现同时有两个程序操作临界区的情况发生。
    while (true) {
        asm volatile(
            "lock cmpxchg %2, %1"
            : "=a"(expected), "+m"(mutex->value)
            : "r"(new_value), "0"(expected)
            : "memory");

        // 如果cmpxchg成功，则mutex->value已经从0变成了1
        if (expected == 0) {
            break;  // 成功获取锁，退出循环
        }

        // 如果未成功获取锁，任务进入阻塞
        task_block(current, &mutex->waiters, TASK_BLOCKED);
        expected = 0;  // 重置expected为0，用于下次比较
    }
}

void mutex_unlock(mutex_t* mutex){
    // 解锁也不需要关中断，一种情况：mutex--后瞬间被中断，新任务又恰好占用mutex，
    // 调度时阻塞任务被恢复会再次判断mutex是否被占用，并再次进入阻塞。
    task_t* current = get_current();
    
    // 直接减小mutex->value的值，解锁
    asm volatile(
        "lock dec %0"
        : "+m"(mutex->value)
        : 
        : "memory");

    task_unblock(&mutex->waiters);
}
```



# Lock 锁

## 自旋锁

同一时间只有一个进程在运行，此时大多数情况都不会出现问题，但是存在 **判断 - 获取锁** 的过程中被中断的情况，此时调度到其他程序就会出现同时有两个程序操作临界区的情况发生。

不过在单线程操作系统上，自旋锁的意义不大，互斥量是更好的选择。自旋锁主要目的是在多线程操作系统上，减少进程阻塞-调度-恢复的上下文开销，所以不进行实现了。

> 在单线程操作系统中，同一时刻只会有一个进程在运行。因此，自旋锁的自旋行为在这种情况下毫无意义。因为自旋锁的主要机制是**不断轮询等待锁的释放**，但是在单线程系统中，轮询等待的线程是唯一运行的线程，它无法让出CPU时间给其他进程来释放锁，结果就是进入了**无谓的自旋等待**。

## 读写锁

消费者生产者模型，也就是value是int而不是bool

```c
// 读写锁
typedef struct wrlock_t{
    bool writer;    // 写者状态
    u32 readers;    // 读者计数
    list_t waiters;
} wrlock_t;
```

读写锁的实现原理其实很简单，原子变量是writer，也就是写者状态。读者会尝试获取writer来测试是否有写者，之后立即释放，代表此时都是读者。写者则需要获取writer来阻塞所有后续读者，之后等待读者们逐步退出临界区，直到readers归零，便可以顺利进入临界区，此时不可能有其他读者或者写者再次进入临界区修改readers，因为writer已经被持有。

这种设计可以保证一个原子变量 writer 可以控制所有的读者写者，并保证写者不会被饿死。

**linux 中的 mutex 是混合自旋与阻塞实现的，短时间自旋尝试获取锁，若长时间竞争失败，则进行阻塞**。

但是读写模型有一个问题，读者可以有很多个，而如果通通加入阻塞队列，则会导致读者如同队列一般一个个执行，而如果一次性释放所有队列，则可能释放新的写者，所有进程又要重新进入一次队列，效率肯定不行。

考虑到目前单核单线程的操作系统实际情况，需要用自旋（不是自旋锁），即while(value)的形式，不能使用阻塞的方式，否则无法决定写者进程被唤醒的时机，写者会被饿死。

# Multiboot2 头

## grub 支持

multiboot 是 grub 的标准，接下来接入grub，需要让内核支持 multiboot。

grub 是 linux 常用的引导程序也就是 bootloader，所以它替代了手写的 bootloader两部分，可以自动加载内核并且开启保护模式，因此可以直接从内核镜像的head 的部分开始，这个 multiboot2 的头就需要写道 head 的开头。

> 要支持 multiboot2，内核必须添加一个 multiboot 头，而且必须再内核开始的 32768(0x8000) 字节，而且必须 64 字节对齐；
>
> | 偏移  | 类型 | 名称                     | 备注 |
> | ----- | ---- | ------------------------ | ---- |
> | 0     | u32  | 魔数 (magic)             | 必须 |
> | 4     | u32  | 架构 (architecture)      | 必须 |
> | 8     | u32  | 头部长度 (header_length) | 必须 |
> | 12    | u32  | 校验和 (checksum)        | 必须 |
> | 16-XX |      | 标记 (tags)              | 必须 |
>
> - `magic` = 0xE85250D6
> - `architecture`:
>   - 0：32 位保护模式
> - `checksum`：与 `magic`, `architecture`, `header_length` 相加必须为 `0`

> ## 参考文献
>
> - <https://www.gnu.org/software/grub/manual/grub/grub.html>
> - <https://www.gnu.org/software/grub/manual/multiboot2/multiboot.pdf>
> - <https://intermezzos.github.io/book/first-edition/multiboot-headers.html>
> - <https://os.phil-opp.com/multiboot-kernel/>
> - <https://bochs.sourceforge.io/doc/docbook/user/bios-tips.html>
> - <https://forum.osdev.org/viewtopic.php?f=1&t=18171>
> - <https://wiki.gentoo.org/wiki/QEMU/Options>
> - <https://hugh712.gitbooks.io/grub/content/>

添加 makefile 制作iso

```makefile
# 链接时控制内存布局，增加 multiboot2 header
LDFLAGS:= -m elf_i386 \
		-static \
		-Ttext $(ENTRYPOINT)\
		--section-start=.multiboot2=$(MULTIBOOT2)
LDFLAGS:=$(strip ${LDFLAGS})

# 创建 grub 引导的系统 iso 镜像文件
$(BUILD)/kernel.iso: $(BUILD)/kernel.bin $(SRC)/utils/grub.cfg
	grub-file --is-x86-multiboot2 $< # 检查 iso 是否合法
	mkdir -p $(BUILD)/iso/boot/grub
	cp $< $(BUILD)/iso/boot
	cp $(SRC)/utils/grub.cfg $(BUILD)/iso/boot/grub
	grub-mkrescue -o $@ $(BUILD)/iso
```

grub-mkrescue需要安装两个工具

```bash
sudo apt-get install xorriso # 用于在 UNIX 类操作系统（如 Linux）中创建、读取和修改 ISO 9660 文件系统映像，也就是常用于光盘或 USB 启动盘的 ISO 镜像文件。
sudo apt-get install mtools # 用于创建 可启动的 ISO 镜像。如果你正在构建操作系统内核或制作 LiveCD、安装盘等，xorriso 可能是制作 ISO 镜像的一部分工具链。
```

grub.cfg

```ini
set timeout=3
set default=0

menuentry "LightOS"{
    multiboot2 /boot/kernel.bin
}
```

grub顺利启动后，由于之前的memory_init需要检查bootloader提供的内核魔术，但是由于grub替代了bootloader，因此魔术会校验失败。

## grub 引导内核后的 i386 状态

- EAX：魔数 `0x36d76289`
- EBX：包含 bootloader 存储 multiboot2 信息结构体的，32 位物理地址
- CS：32 位 可读可执行的代码段，尺寸 4G
- DS/ES/FS/GS/SS：32 位可读写的数据段，尺寸 4G
- A20 线：启用
- CR0：PG = 0, PE = 1，其他未定义
- EFLAGS：VM = 0, IF = 0, 其他未定义（VM: Virtual 8086 Mode，没必要支持16位实模式程序，且8086要用到前1M给16实模式应用）
- ESP：内核必须尽早切换栈顶地址
- GDTR：内核必须尽早使用自己的全局描述符表
- IDTR：内核必须在设置好自己的中断描述符表之前关闭中断

因此需要修改head.asm，设置gdt、段寄存器、esp，以及修改memory_init的启动魔术判断（设置memory_base与memory_size）。

# 键盘中断&中断思考

0x21 键盘中断向量，实现起来也非常简单，就是将0x21的interrupt_handler填好就可以。

| 端口 | 操作类型 | 用途       |
| ---- | -------- | ---------- |
| 0x60 | 读/写    | 数据端口   |
| 0x64 | 读       | 状态寄存器 |
| 0x64 | 写       | 控制寄存器 |

遇到了一个问题，当前的实现逻辑是，键盘中断会将其按键数据打印到console上，但是console的打印目前采用的是 mutex 的形式，并不禁用中断。

如果一旦在打印的过程中触发中断，就会出现键盘中断后再次尝试获取该 mutex，但是由于current 不变，因此会将自己这个进程加入阻塞，之后任何人都拿不到这个 mutex 了，其余调度全阻塞，只剩下idle了，**死锁了**！

所以中断有了中断上下文的要求，这与之前 windows 驱动开发中遇到的规则基本一致。

* 中断程序运行在中断上下文中。
* 中断上下文中不可获取可能导致阻塞的资源或锁（如 mutex ），只能使用spinlock。
* 可采用中断延后程序，将中断分割执行，保持快速相应和后续的执行功能可以进行安全中断。

其中，对于中断栈的选择是有说法的，类似这种键盘中断的话，需要将栈切换到 IRQ Stack 保证中断不影响其他内核态任务执行。

> **中断发生时的栈选择**：
>
> - 如果中断在**用户态**发生，处理器同样会通过 TSS 自动切换到当前进程的内核栈（`ESP0`）。
> - 如果中断在**内核态**发生（例如操作系统代码正在执行时），CPU不会自动切换栈，而是继续使用当前正在执行的内核栈。
>
> **中断栈管理的挑战**：
>
> - **时钟中断** 和 **外部设备中断** 是系统全局的，并不专属于某个具体进程。为了确保中断处理不会使用某个进程特定的内核栈，操作系统往往会实现独立的中断栈（如 Linux 的 IRQ Stack）。这样，即使中断发生在内核态，也可以安全切换到专用的中断栈，以防止影响内核态其他任务的执行。

所以现在有两种思路：

1. 在console中直接关闭中断，简单好用，但是影响系统准时性，还会丢中断。
2. 实现中断代理进程，将中断分为两部分，上下半通过链表通信，下半是一个（或多个临时创建的）代理进程，负责代理执行中断下半部分（如处理网络数据包，更新系统状态），下面是 linux 的实现原理：

> #### 1. **SoftIRQ（软中断）**
>
> 软中断是一种常见的机制，它允许延迟执行中断处理的某些部分。操作系统会注册不同类型的软中断，当需要处理时，调度器会根据优先级安排软中断执行。
>
> **Linux的SoftIRQ机制**：Linux使用`SoftIRQ`来处理网络数据包接收、任务调度等需要延迟处理的任务。中断发生时，驱动程序会触发相关的`SoftIRQ`，并将信息推送到队列中。在稍后的时间段，操作系统会调度`SoftIRQ`，并从队列中取出数据进行处理。
>
> #### 2. **Tasklets**
>
> Tasklets 是 Linux 中更高级的一种中断延后处理机制，实际上是软中断的具体实现之一。每个 tasklet 都是一个轻量级的延迟任务。它们通常用于设备驱动程序，在中断发生时将任务推迟到稍后执行。
>
> **工作方式**：上半部分的中断处理程序可以调度一个 tasklet，tasklet 会在适当的时间由操作系统安排执行。在执行 tasklet 时，中断是允许的，因此不会阻塞其他中断的执行。
>
> #### 3. **Workqueues（工作队列）**
>
> 工作队列允许内核将较复杂的任务推迟到内核线程中执行。这是操作系统为内核提供的一种延后处理机制，特别适用于那些需要内核进程上下文才能完成的工作。
>
> **Linux中的工作队列**：与 tasklet 和 SoftIRQ 不同，工作队列运行在进程上下文中，因此它们可以睡眠并等待资源。在中断发生后，内核驱动可以使用工作队列来推迟需要长时间或复杂计算的任务。

但是，之所以会出现这个问题，是因为我在中断处理程序中调用了console的打印，正常来讲系统中断不应该也不会触发这类操作（即在中断上下文中尝试控制台打印），因此可以暂时略过这个问题，之后将键盘中断加入消息队列后，打印消息就是由进程完成的了，那时自然不会出现这类问题。

此外，上下半的通信可以采用环形队列的方式进行，即分别设置head和tail指向环形队列消费者生产者的操作位置，由于汇编指令是CPU最小单元（微指令不会被中断打断），因此可以确保head和tail的操作是原子的（确保单一汇编指令操作），此时就可以保证生产者消费者的操作可以并发，不用担心消费者处理队列时候突发中断后生产者继续写入队列的问题。

这部分内容就交给后面写键盘中断的消息队列时再写了，目前不对该问题进行处理，因为console中使用互斥量是一个很合理的选择。

# 键盘驱动

[从按键到响应，键盘的底层原理是什么？](https://zhuanlan.zhihu.com/p/367332722)

简单来说，操作系统需要处理 XT 扫描码，也就是第一套扫描码。键盘中断流程如下：

1. 8048（键盘，AT，第二套扫描码）
2. 8042（Personal System/2，为了保持兼容性，将 AT 扫描码转换为第一套也就是 XT 扫描码）
3. PIC（8259A）
4. CPU 中断响应

用 vmware 测试了一下，发现果然键盘 led 不亮，led 控制居然是操作系统和bios需要分别实现的。

整个驱动实现起来也比较简单。

状态寄存器：

0. 输出缓冲区状态：1 表示输出缓冲区满
1. 输入缓冲区状态：1 表示输入缓冲区满
2. 系统标志位：加电时置为 0，自检通过时置为 1
3. 命令/数据位：1 表示输入缓冲区的内容是命令，0 表示输入缓冲区的内容是数据
4. 1 表示键盘启用，0 表示键盘禁用
5. 1 表示发送超时 （存疑）
6. 1 表示接受超时 （存疑）
7. 奇偶校验出错

# 循环队列

```bash
| ---------- | ----------- | ----------- | .... loop back
     emtpy     xxxxxxxxxxx ->   empty
         tail(out)      head(in)
```

了解现代操作系统的终端概念：

## tty(Teletypewriter) 终端设备与标准输入

> ###  操作系统如何在键盘和Shell之间工作
>
> 操作系统在键盘和Shell之间的工作涉及几个层次：**硬件设备管理**、**输入输出控制**、以及**应用程序（如Shell）的交互**。具体步骤如下：
>
> #### 1 键盘输入管理（设备驱动层）
>
> - **键盘驱动程序**： 键盘输入首先由键盘控制器检测并触发硬件中断。操作系统的键盘驱动程序负责处理这一中断事件，读取来自键盘的**扫描码**，并将其转换为字符或控制码。
>   1. 用户按下一个键，键盘硬件会发送扫描码到键盘控制器。
>   2. 键盘控制器通过触发中断（通常是IRQ1）通知CPU有输入事件发生。
>   3. 操作系统调用相应的**中断服务程序（ISR）**，从键盘控制器读取扫描码并将其转换为字符，处理完成后将字符放入缓冲区中。
>
> #### 2 终端驱动程序（TTY 层）
>
> - **TTY（终端）驱动程序**： 操作系统使用终端驱动程序（也称为TTY驱动程序）处理字符输入和输出。终端驱动程序负责管理键盘、显示器等I/O设备的交互。它实现了行缓冲、字符回显等功能。
>   1. TTY驱动将键盘驱动程序获取到的字符存入一个行缓冲区中，并处理回显，将输入的字符显示在屏幕上（可以关闭，如输入密码时会取消输入的回显）。
>   2. 当用户按下回车键时，TTY驱动会将一整行输入从缓冲区提交给正在运行的应用程序（如Shell）。
>   3. 它还处理一些特殊字符，如`Ctrl+C`用于中断进程，`Ctrl+D`用于结束输入。
> - **标准输入输出接口**： TTY驱动程序通过提供标准的输入（`stdin`）和输出（`stdout`）接口与用户空间程序（如Shell）进行通信。应用程序调用系统调用（如`read()`和`write()`）与终端进行交互，读取用户输入并输出结果。
>
> #### 3 Shell的交互
>
> - **Shell的工作**： Shell是操作系统中的命令行解释器，负责接收用户输入的命令并执行相应的操作。当用户通过键盘输入命令并按下回车键时，终端驱动程序会将输入的数据提交给Shell。Shell执行以下步骤：
>   1. **读取输入**：Shell通过调用`read()`系统调用从标准输入中读取一整行输入数据（由回车键触发）。
>   2. **解析命令**：Shell解析输入的命令行，将其拆分为命令和参数。
>   3. **执行命令**：根据用户输入的命令，Shell调用操作系统提供的API或执行外部程序（例如通过`exec()`族系统调用执行外部可执行文件）。
>   4. **显示结果**：命令执行后，结果通过标准输出（`stdout`）返回给终端驱动，终端驱动程序负责将结果显示在屏幕上。

读取键盘应该是互斥的，即同一时间只能有一个程序读到键盘输入

> 键盘输入通常通过文件描述符（`stdin`，即标准输入）来访问。在Unix和Linux等操作系统中，终端设备（包括键盘）被抽象为文件，通过系统调用进行访问。每个终端设备会与当前运行的程序关联（通常是当前的前台进程组），该进程组内的进程有权限读取输入。
>
> - **前台进程组**： 操作系统为每个终端（TTY）维护一个**前台进程组**，前台进程组中的进程有权限与终端进行交互，包括读取键盘输入。只有前台进程组内的一个进程能够实际从标准输入中读取数据。后台进程组无法读取标准输入。
>
>   这种设计确保在多进程环境中，当用户与某个Shell或应用交互时，只有该应用所在的前台进程组能够读取键盘输入，防止其他进程干扰用户输入。

## 在中断里触发 yield 的问题

进程 schedule 中保存了 eflags，所以中断配置会被保留并于进程绑定。

发现了一个问题，写了个逻辑，在键盘中断处理函数中，将字符加入循环队列后，尝试唤醒 keyboard_read 的进程，但是最开始没有考虑到这是中断，unblock 中 yield 出去了。（当然，这个yield写的也不好，因为是在内核中直接调用的，因此后面都改成了schedule，少一次系统调用栈）

```c
if (fg_task != NULL){
    kfifo_put(&kb_fifo, ch);
    task_intr_unblock_no_waiting_list(fg_task);
    fg_task = NULL;
}
```

这个键盘中断发生的时机一般在 idle 中，由于没有增加中断上下文，所以当fg_task执行完毕后（重新赋值非NULL）继续阻塞，idle 将被继续调度，并从 `fg_task = NULL;` 继续执行，这样刚刚赋值好的 fg_task 又被清空，与逻辑预期不符。

思考了一下，在中断中不应该使用 schedule 类进行切换，应尽可能快的处理完立刻中断返回，返回到被中断的进程中继续执行（该进程可以是任意的进程，因为中断可以任意时间发生）。clock时钟中断是个例外，其中必须要进行schedule才能保证程序按照时间片来执行，因此一个特点就是，被中断的进程被重新调度回来时，会从时钟中断的 schedule 继续执行，之后再中断返回，这部分是关中断的状态，不过目前感觉不会有什么影响。

# 任务状态段

## 内核态再次触发中断的栈情况

一般来说，内核不应该触发系统调用 int 0x80，但是之前测试一直是这种方式，于是探究了一下中断时的硬件是如何操作内核栈的（其他内核态程序运行时触发的外部中断和异常应该都一样）。结果是，内核态程序触发的中断，**不会**修改栈指针，而是继续使用当前的栈，并且将中断参数直接压入当前栈中。即：**如果 CPL 没有发生变化（内核态下中断），不会对 `ESP` 和 `SS` 进行任何修改，寄存器 `ESP` 和 `SS` 保持不变。**当然 ebp 观察下来也不会变。

再列一下中断压栈参数：

* **EFLAGS**：中断前的标志寄存器值。
* **CS（代码段寄存器）**：中断前的代码段选择子。
* **EIP（指令指针寄存器）**：中断前的指令地址。

## IA-32 TSS

![image-20241014165133259](D:\Markdown\OpreatingSystem\LightOS\develop_dialog\markdown_img\image-20241014165133259.png)

TR （Task Register）寄存器用来储存 TSS Descriptor，也就是任务状态段描述符，其中，Type 中的 B 位标识任务是否繁忙。CPU 使用 ltr 指令加载 TSSD

```bash
ltr
```



![image-20241014191604795](D:\Markdown\OpreatingSystem\LightOS\develop_dialog\markdown_img\image-20241014191604795.png)

**实际上，在使用软件任务上下文切换的形势下，使用 TSS 的目的就是为了通过设置 ss0 和 esp0，让用户态程序可以由中断自动转为内核态（栈）。**

一些用不到的特殊 TSS 字段标识。

* NT（Nested Task）标识嵌套任务
* IOPL（I/O privilege Level）I/O 优先级，当前可以执行 I/O 指令的任务的特权级。设置为0表示只有0特权级程序可以进行I/O操作。不过 Intel 在 TSS 中提供了 iobase（I/O位图基地址，端口号位图），使得用户态任务可以执行 I/O 指令，不过实际上用不到。

# 进入用户模式

**还有一个问题没有解决**，就是如何让不同的用户态进程中断时进入其独立的内核栈中？如果TSS是全局的 GDT[3] 项，并已经 ltr 加载进 task register 中了，那么我目前只想到了两种方案，但我很好奇 linux 0.11 和现代多核操作系统如何实现？

1. 进程切换时顺带修改 tss 中的 esp0 指向其独立的内核栈
2. 共用内核栈进入时利用禁用中断期，将栈迁移到独立的内核栈

## move_to_user

终于来到了 move_to_user 的部分，真正将 init 进程置于 Ring3 执行。

与之前的中断上下文不同，如果是从用户中断进入内核，则硬件自动压栈了ss3和esp3两个用户态栈值。move_to_user 自然要模拟这样一个栈后 ROP 返回 Ring3。

![image-20241014204246768](D:\Markdown\OpreatingSystem\LightOS\develop_dialog\markdown_img\image-20241014204246768.png)

现在实现思路比较怪，有两条思路。

1. linux 0.11 是直接move_to_user后进行一个fork()，fork出来的一个作为 idle，另一个是init，但是两个问题，第一个ring3为什么可以执行idle和init部分的内核代码，第二个，它是什么时候赋值的task_list，分别初始化的idle和init进程？
2. onix则采用主动创建的方式，分别创建了idle和init两个进程，并且将idle和init都拆成了两个部分，第一部分是内核代码，然后第二部分（目前没有文件系统，所以关闭了GDT中禁止用户态执行内核代码的权限位 USER_CODE）负责写入中断栈，iret后作为ring3的代码起始点执行。

根据正常操作系统设计，init进程应该是来自文件系统的一个二进制程序，并且确实需要保护系统内核代码不能被用户态访问执行，所以果断第二种方案，但是要稍稍修改一下这个逻辑，通过create_task创建了idle和init_kthread后，将当前状态设置为idle，并继续执行内核初始化其他工作，之后跳转至idle中执行。其中idle需要在函数入口处重设栈指针到idle自己的内核栈（开中断，为了idle被调度回来的时候也会根据tss.eflags自动开中断），开始循环调度。此时才会调度到init_kthread中，之后move_to_user中断返回到init_uthread中（没有文件系统，暂时以内核代码实现），开始ring3的执行！

## 重启和PF两个问题

如果在init中尝试yiled系统调用进内核，则在实测发现两个问题：

1. 中断发生后直接重启。

   > 由于之前发生过这个问题，所以猜测仍是esp赋值0导致的。根据用户态中断原理，其中有一环是根据 tr 寄存器中的段选择子，找到全局tss，并赋值将tss.esp0赋值到esp中，从而让进程中断后进入到正确的内核栈。但是tss中尚未设置esp0，于是考虑到每个进程都有自己独立的内核栈，因此将全局tss.esp0的赋值工作放入了switch_to中，切换进程自动赋值全局tss的esp0。**但是，这样不能支持多核！**不过多核应该要改很多很多东西，目前已知的多核APIC以及每个CPU都有一个独立的内核栈，多核系统与GDT（全局TSS）的关系还有待考证，有可能一个核一个全局TSS，这样的话这种方案也不是不行（当然另一种方案就是统一中断进入相同内核再栈迁移，但是不够艺术）。

   

2. init中yield进schedule中的switch_to（如果不跳过进程0），发生PF，eip是栈上值（0x104000+）

   > 这居然是一个老的bug，switch_to的切换没有考虑栈帧问题！切换调用了save_state和load_state，会增加栈帧入栈，load的跳转位置却没有栈帧的清理，因此果断放弃函数调用的方式，直接写汇编进switch_to，不引入任何栈帧，即可保证栈结构正常。之所以出现eip是栈上值，就是因为没清理栈帧导致ret了局部变量指针。

发生 PF 的调试方法：

1. 中断到PF的while处，观察调用栈或者修改while变量依次返回到问题点。
2. 根据输出的eip，debug查`-exec info line *0xpppp`，或者去找system.map，看目标位置的函数符号即可。

# printf

底层是 vsprintf 和 write 系统调用，向 fd=1 写入字符（stdin, stdout, stderr），就是一个简单的系统调用。

目前没有文件系统，因此 write 比较 stdout 后直接 console_write。

# Arena（kmalloc+kfree）

> `kmalloc` 的实现是基于 **slab 分配器**（Slab Allocator），也可能是 **slub 分配器** 或 **slob 分配器**，它们是 Linux 内核中不同的内存管理策略。
>
> - **Slab 分配器**:
>   - `kmalloc` 是基于 slab 分配器构建的。Slab 分配器通过缓存对象来减少频繁的内存分配和释放所带来的开销。
>   - Slab 分配器将内存分为多个 slab（页框），每个 slab 包含若干个固定大小的对象。当需要分配内存时，slab 分配器从缓存池中分配一个适合大小的对象。
>   - 它能有效管理小块内存分配请求，如内核经常需要分配和释放的小数据结构。
> - **Slub 分配器**:
>   - 这是 slab 分配器的改进版本，减少了缓存池的复杂性，具有更好的性能表现，尤其是在多处理器系统上。
> - **SLOB 分配器**:
>   - 这是为嵌入式系统或资源有限的系统设计的，能够处理较小的内存分配需求，适用于低开销环境。

不可能手写一个完善的堆管理器，所以怎么简单怎么来。

# 用户内存映射管理

当前 LightOS 的内核内存是前 8M，已经在memory初始化过程中完成了（线性地址 == 物理地址）页表映射，而 8M 之后的物理内存则映射给用户空间。每个进程映射的物理地址不同，即页表不同，所以只需要实现两个函数，分别是：将页表映射到物理内存、取消映射，并于 PF 中自动绑定 current 程序的页表进行映射即可。当前程序的页表应该动态申请，并保存在 task_t 的 pde 字段中（pde / pte / offset）。

当前操作系统为了方便，让内核物理地址和线性地址相等，也就是说内核位于低地址范围，这与现代操作系统不符，现代操作系统包括32位的都是内核虚拟地址在高地址范围，并且通过 PT 的共享，可以让所有用户态程序共享内核地址。

不过实际上用户态程序无法访问高地址空间，因此我认为这个共享是x86独特的、由cr3机制决定的。即因为用户态程序和中断进入内核后的cr3不变，也就是页表不变，此时为了能够访问内核态地址，必须将内核地址加入用户态页表的pd中（高地址的1G需要添加PD后256个项目），而PT是共享的，从而实现内核地址共享，且不会对内存造成负担（pd是必要的开销）。ARM由于存在ttbr0和ttbr1的区别，这样就将用户页表和内核页表分割开，自然也就不需要在用户态的pd中加入内核的地址映射了。

## BUGGGG!

真实的一个老bug调了一天，列入24年最离谱bug记录！

几个月前，在分页功能实现之前，memory_map_init 中写错了一个变量：

```c
memset((void*)mem_map_pages, 0, mem_map_pages * PAGE_SIZE);
```

实际上是清空mem_map，结果把页数量传进去清空了，这个数量很少，当前os的值是0x2。但是非常巧的是，后面写好的分页初始化 mapping_init 又恰好把页表这个位置填回去了，所以这个bug就一直潜伏没出现。

现在我为了将虚拟地址映射到高地址，且想尽可能快点使用全局变量，于是移动了mapping_init的位置，将它提前到了memory_map_init前面了。嗯，，，，炸的不留余地，而且 qemu 中如果出现了硬件机制的错误，会直接重启，根本没有报错和 log 可供排查。而关键是我并没有办法定位到这个问题，因为内核链接已经改到了高地址范围，此时 gdb 无法加载这个符号文件到低地址，自然无法下断点。

我一直以为是gdb_init或者是mapping_init本身有问题，也是查了改了很久，都没有办法定位到问题，一运行到enable_page，写cr0启动分页就会重启。

后来想了一个招，同时生成两个kernel.bin，一个高一个低，然后gdb加载这个低的bin，就可以在分页前下断点了，然而没什么卵用，出问题的根本不是这里。

再后来尝试在原版中测试一下，为什么原本启动分页就没问题呢？之后诡异的发现移动了mapping_init的位置，就会崩溃重启，通过逐行移动测试发现了这个抽象的、根本想不到的memery_map_init居然会影响mapping_init？就算删除mapping_init中的mem_map操作，也一样崩，嗯，仔细审了一边代码，终于揪出来了。

这个bug从发现重启问题，到解决花了8个小时，藏得很深，很难发现，又恰巧位于分页前，增加了很多调试困难，列入24年最离谱bug记录！

## 虚拟地址高地址实现

在mapping前，必须要实现一个临时的gdt和gdt_ptr，仅设置内核代码段和数据段，但是要保证其地址是安全的。当前分别位于0x2a000和0x29000。用一下就可以了，后面马上被正常的全局gdt覆盖，不用太纠结。

此外，由于设置mapping的页会覆盖bios启动的内存检测参数数组，因此手动拷贝到0x30000位置（不够精妙，先简单实现一下）。

经过测试，可以顺利迁移到高地址空间！并且gdb可以在mapping_init+跳转后正常中断！

后续修复了许多直接物理内存访问的代码，如`KERNEL_MAP_BITS`以及申请页返回指针需要增加偏移等。

但是目前缺乏 grub 的 multiboot2 规则支持，目前grub应该会自动根据镜像指定的地址加载到3G位置，但是这样启动会 out of memory。可能需要手动指定内核加载地址为0x10000，并设置入口为0x10040。

重新放一张内存布局图

![highlight](.\markdown_img\highlight.png)











