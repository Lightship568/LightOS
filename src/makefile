BUILD:= ../build
SRC:= .
# MULTIBOOT2:= 0x10000
# ENTRYPOINT:= 0x10040 # multiboot2 + 64 bytes

# 内核迁移到虚拟地址高地址空间
MULTIBOOT2:= 0xc0010000
ENTRYPOINT:= 0xc0010040

CFLAGS:= -m32
# CFLAGS:= -Qn 					#去掉gcc版本信息
CFLAGS+= -fno-builtin 			#不需要gcc内置函数（比如memcpy）
CFLAGS+= -nostdinc 				#不需要标准头文件（c的）
CFLAGS+= -nostdlib 				#不需要标准库
CFLAGS+= -fno-pic 				#不需要 position independent code
CFLAGS+= -fno-pie 				#不需要 position independent excutable
CFLAGS+= -fno-stack-protector	#不需要栈保护
# CFLAGS+= -fomit-frame-pointer	#不需要栈帧
# CFLAGS+= -mpreferred-stack-boundary=2	#不需要栈对齐
# CFLAGS+= -fno-asynchronous-unwind-tables	#不需要 CFI 信息
CFLAGS+= -DLIGHTOS				#定义LIGHTOS

CFLAGS:=$(strip ${CFLAGS}) 		#去除\t

DEBUG:= -g
INCLUDE:= -I$(SRC)/include

# 模式规则
# boot(boot/loader.asm)
$(BUILD)/boot/boot.bin: $(SRC)/boot/boot.asm
	$(shell mkdir -p $(dir $@))
	nasm -f bin $(DEBUG) $< -o $@
$(BUILD)/boot/loader.bin: $(SRC)/boot/loader.asm
	$(shell mkdir -p $(dir $@))
	nasm -f bin $(DEBUG) $< -o $@
# head.asm
$(BUILD)/boot/head.o: $(SRC)/boot/head.asm
	$(shell mkdir -p $(dir $@))
	nasm -f elf32 $(DEBUG) $< -o $@

# init/main.c init/init.c...
$(BUILD)/kernel/%.o: $(SRC)/init/%.c
	$(shell mkdir -p $(dir $@))
	gcc $(CFLAGS) $(DEBUG) $(INCLUDE) -c $< -o $@

# kernel (*.c)
$(BUILD)/kernel/%.o: $(SRC)/kernel/%.c
	$(shell mkdir -p $(dir $@))
	gcc $(CFLAGS) $(DEBUG) $(INCLUDE) -c $< -o $@

# kernel (*.asm)
$(BUILD)/kernel/%.o: $(SRC)/kernel/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f elf32 $(DEBUG) $< -o $@

# lib (*.c)
$(BUILD)/lib/%.o: $(SRC)/lib/%.c
	$(shell mkdir -p $(dir $@))
	gcc $(CFLAGS) $(DEBUG) $(INCLUDE) -c $< -o $@

# lib (*.asm)
$(BUILD)/lib/%.o: $(SRC)/lib/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f elf32 $(DEBUG) $< -o $@
	
# fs (*.c)
$(BUILD)/fs/%.o: $(SRC)/fs/%.c
	$(shell mkdir -p $(dir $@))
	gcc $(CFLAGS) $(DEBUG) $(INCLUDE) -c $< -o $@


# 将 lib 下用户态 .o 打包成 libc
$(BUILD)/lib/libc.o: 			\
	$(BUILD)/lib/crt.o 			\
	$(BUILD)/lib/crt1.o			\
	$(BUILD)/lib/string.o		\
	$(BUILD)/lib/vsprintf.o		\
	$(BUILD)/lib/stdlib.o		\
	$(BUILD)/lib/syscall.o		\
	$(BUILD)/lib/printf.o		\
	$(BUILD)/lib/assert.o		\
	$(BUILD)/lib/time.o			\


	ld -m elf_i386 -r $^ -o $@

# 链接时控制内存布局，增加 multiboot2 header
LDFLAGS:= -m elf_i386 \
		-static
LDFLAGS+= -T$(SRC)/utils/kernel.ld # 引用链接器脚本文件
# LDFLAGS+= -Ttext $(ENTRYPOINT) --section-start=.multiboot2=$(MULTIBOOT2)

LDFLAGS:=$(strip ${LDFLAGS})

# kernel
# -T 重定位text入口地址
$(BUILD)/kernel.bin: \
	$(BUILD)/boot/head.o		\
	$(BUILD)/kernel/main.o		\
	$(BUILD)/lib/io.o			\
	$(BUILD)/kernel/console.o	\
	$(BUILD)/lib/string.o		\
	$(BUILD)/lib/vsprintf.o		\
	$(BUILD)/lib/print.o		\
	$(BUILD)/kernel/assert.o	\
	$(BUILD)/lib/debug.o		\
	$(BUILD)/kernel/global.o	\
	$(BUILD)/kernel/interrupt.o	\
	$(BUILD)/kernel/interrupt_handler.o	\
	$(BUILD)/lib/stdlib.o		\
	$(BUILD)/kernel/task.o		\
	$(BUILD)/kernel/clock.o		\
	$(BUILD)/kernel/rtc.o		\
	$(BUILD)/kernel/time.o		\
	$(BUILD)/lib/time.o			\
	$(BUILD)/kernel/memory.o	\
	$(BUILD)/lib/bitmap.o		\
	$(BUILD)/lib/syscall.o		\
	$(BUILD)/kernel/gate.o		\
	$(BUILD)/lib/list.o			\
	$(BUILD)/kernel/schedule.o	\
	$(BUILD)/lib/mutex.o		\
	$(BUILD)/kernel/keyboard.o	\
	$(BUILD)/lib/kfifo.o		\
	$(BUILD)/kernel/init.o		\
	$(BUILD)/lib/printf.o		\
	$(BUILD)/lib/arena.o		\
	$(BUILD)/kernel/ide.o		\
	$(BUILD)/kernel/device.o	\
	$(BUILD)/kernel/cache.o		\
	$(BUILD)/fs/super.o 		\
	$(BUILD)/fs/bmap.o 			\
	$(BUILD)/fs/inode.o 		\
	$(BUILD)/kernel/system.o 	\
	$(BUILD)/fs/namei.o 		\
	$(BUILD)/fs/file.o 			\
	$(BUILD)/fs/stat.o			\
	$(BUILD)/fs/dev.o			\
	$(BUILD)/kernel/ramdisk.o	\
	$(BUILD)/kernel/execve.o	\
	$(BUILD)/kernel/serial.o	\
	$(BUILD)/fs/pipe.o			\
	$(BUILD)/kernel/timer.o		\


	$(shell mkdir -p $(dir $@))
	ld ${LDFLAGS} $^ -o $@

# 方便loader加载内核
$(BUILD)/system.bin: $(BUILD)/kernel.bin
	objcopy -O binary $< $@
# 生成kernel符号表
$(BUILD)/system.map: $(BUILD)/kernel.bin
	nm $< | sort > $@



include utils/image.mk

include utils/buildroot.mk

include utils/cmd.mk 