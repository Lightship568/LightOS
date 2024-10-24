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


	$(shell mkdir -p $(dir $@))
	ld ${LDFLAGS} $^ -o $@

# 方便loader加载内核
$(BUILD)/system.bin: $(BUILD)/kernel.bin
	objcopy -O binary $< $@
# 生成kernel符号表
$(BUILD)/system.map: $(BUILD)/kernel.bin
	nm $< | sort > $@


# 将yes直接输入到bximage命令中，让其自动更新覆盖原来的镜像文件
$(BUILD)/LightOS.img: $(BUILD)/boot/boot.bin $(BUILD)/boot/loader.bin $(BUILD)/system.bin $(BUILD)/system.map
	# yes | bximage -q -hd=16 -mode=create -sectsize=512 -imgmode=flat $@ 
	yes | bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat $@ # ubuntu22.04的bximage -mode 改为 -func
	dd if=$(BUILD)/boot/boot.bin of=$@ bs=512 count=1 conv=notrunc
	dd if=$(BUILD)/boot/loader.bin of=$@ bs=512 count=4 seek=1 conv=notrunc # boot.asm 从1开始读4个扇区
# 测试 system.bin 小于 100k，否则增加下面的 count
	test -n "$$(find $(BUILD)/system.bin -size -100k)"
	dd if=$(BUILD)/system.bin of=$@ bs=512 count=200 seek=5 conv=notrunc # 5扇区开始写200个扇区，100k


# 创建 grub 引导的系统 iso 镜像文件
$(BUILD)/kernel.iso: $(BUILD)/kernel.bin $(SRC)/utils/grub.cfg
	rm -f $(BUILD)/kernel.iso
	grub-file --is-x86-multiboot2 $< # 检查 iso 是否合法
	mkdir -p $(BUILD)/iso/boot/grub
	cp $< $(BUILD)/iso/boot
	cp $(SRC)/utils/grub.cfg $(BUILD)/iso/boot/grub
	grub-mkrescue -o $@ $(BUILD)/iso



.PHONY: clean
clean:
	rm -rf $(BUILD)

test: $(BUILD)/LightOS.img

.PHONY: bochs
bochs: $(BUILD)/LightOS.img
	bochs -q -f ../bochs/bochsrc

# -s：监听1234端口，-S：GDB没有连接QEMU之前不执行
# -c 硬盘启动 -m 32M的内存大小

QEMU := qemu-system-i386 \
	-m 32M \
	-audiodev pa,id=hda,server=/run/user/1000/pulse/native \
	-machine pcspk-audiodev=hda \
	-rtc base=localtime	\

QEMU_DISK:=-boot c\
	-drive file=$(BUILD)/LightOS.img,if=ide,index=0,media=disk,format=raw

QEMU_CDROM:=-boot d\
	-drive file=$(BUILD)/kernel.iso,media=cdrom\

QEMU_DEBUG:=-s -S

.PHONY: qemu
qemu: $(BUILD)/LightOS.img
	$(QEMU) $(QEMU_DISK)

.PHONY: qemug
qemug: $(BUILD)/LightOS.img
	$(QEMU) $(QEMU_DISK) $(QEMU_DEBUG)

.PHONY: qemu-grub
qemu-grub: $(BUILD)/kernel.iso
	$(QEMU) $(QEMU_CDROM)

.PHONY: qemug-grub
qemug-grub: $(BUILD)/kernel.iso
	$(QEMU) $(QEMU_CDROM) $(QEMU_DEBUG)

.PHONY: iso
iso: $(BUILD)/kernel.iso

$(BUILD)/LightOS.vmdk: $(BUILD)/LightOS.img
	qemu-img convert -O vmdk $< $@
.PHONY: vmdk
vmdk: $(BUILD)/LightOS.vmdk