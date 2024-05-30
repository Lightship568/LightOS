BUILD:= ../build
SRC:= .
ENTRYPOINT:= 0x10000

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
# boot
$(BUILD)/boot/%.bin: $(SRC)/boot/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f bin $(DEBUG) $< -o $@

# kernel (head.o)
$(BUILD)/kernel/%.o: $(SRC)/kernel/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f elf32 $(DEBUG) $< -o $@

# kernel (*.c)
$(BUILD)/kernel/%.o: $(SRC)/kernel/%.c
	$(shell mkdir -p $(dir $@))
	gcc $(CFLAGS) $(DEBUG) $(INCLUDE) -c $< -o $@

# lib (*.c)
$(BUILD)/lib/%.o: $(SRC)/lib/%.c
	$(shell mkdir -p $(dir $@))
	gcc $(CFLAGS) $(DEBUG) $(INCLUDE) -c $< -o $@

# kernel
# -T 重定位text入口地址
$(BUILD)/kernel.bin: \
	$(BUILD)/kernel/head.o		\
	$(BUILD)/kernel/main.o		\
	$(BUILD)/kernel/io.o		\
	$(BUILD)/kernel/console.o	\
	$(BUILD)/lib/string.o		\
	$(BUILD)/lib/vsprintf.o	\
	$(BUILD)/lib/print.o		\

	$(shell mkdir -p $(dir $@))
	ld -m elf_i386 -static $^ -o $@ -Ttext $(ENTRYPOINT) 

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
	dd if=$(BUILD)/system.bin of=$@ bs=512 count=200 seek=5 conv=notrunc # 5扇区开始写200个扇区，100k

.PHONY: clean
clean:
	rm -rf $(BUILD)

test: $(BUILD)/LightOS.img

.PHONY: bochs
bochs: $(BUILD)/LightOS.img
	# bochs -q -f ../bochs-2.8/bochsrc
	bochs -q -f ../bochs-2.8/bochsrc

# -s：监听1234端口，-S：GDB没有连接QEMU之前不执行
# -c 硬盘启动 -m 32M的内存大小
.PHONY: qemu
qemu: $(BUILD)/LightOS.img
	qemu-system-i386 \
	-m 32M \
	-boot c \
	-hda $<

.PHONY: qemug
qemug: $(BUILD)/LightOS.img
	qemu-system-i386 \
	-s -S \
	-m 32M \
	-boot c \
	-hda $<

$(BUILD)/LightOS.vmdk: $(BUILD)/LightOS.img
	qemu-img convert -O vmdk $< $@
.PHONY: vmdk
vmdk: $(BUILD)/LightOS.vmdk