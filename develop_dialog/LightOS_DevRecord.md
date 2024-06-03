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

为了自动化编译内核，需要使用make工具
```
boot.bin: boot.asm
    nasm -f bin boot.asm -o boot.bin
LightOS.img: boot.bin
    yes | bximage -q -hd=16 -mode=create -sectsize=512 -imgmode=flat LightOS.img # 将yes直接输入到bximage命令中，让其自动更新覆盖原来的镜像文件
    dd if=boot.bin of=LightOS.img bs=512 count=1 conv=notrunc
```

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

## 进入内核

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

![eflags](E:\markdown\OpreatingSystem\LightOS\develop_dialog\markdown_img\eflags.png)

8086中采用了两片8259a可编程控制芯片，每片管理8个中断源，两片级联可以控制64个中断向量，但是 PC/AT 系列兼容机最多能够管理15个中断向量。主片端口0x20，从片0xa0。具体原理可参考onix 032 外中断原理，真的很有意思，这才是计算机科学。

![8259a](E:\markdown\OpreatingSystem\LightOS\develop_dialog\markdown_img\8259a.png)

**sti 和 cli 都是针对外中断的，内中断和异常都是 cpu 片内事件，不受 eflags 中的 if 位控制。**

### 中断上下文

![image-20240602195457164](E:\markdown\OpreatingSystem\LightOS\develop_dialog\markdown_img\interrupt_context)

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





