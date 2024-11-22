# 将yes直接输入到bximage命令中，让其自动更新覆盖原来的镜像文件
$(BUILD)/LightOS.img: $(BUILD)/boot/boot.bin \
	$(BUILD)/boot/loader.bin \
	$(BUILD)/system.bin \
	$(BUILD)/system.map	\
	$(SRC)/utils/LightOS.sfdisk

# ubuntu22.04的bximage -mode 改为 -func
# yes | bximage -q -hd=16 -mode=create -sectsize=512 -imgmode=flat $@ 
	yes | bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat $@
	dd if=$(BUILD)/boot/boot.bin of=$@ bs=512 count=1 conv=notrunc
	dd if=$(BUILD)/boot/loader.bin of=$@ bs=512 count=4 seek=1 conv=notrunc # boot.asm 从1开始读4个扇区
# 测试 system.bin 小于 100k，否则增加下面的 count
	test -n "$$(find $(BUILD)/system.bin -size -100k)"
	dd if=$(BUILD)/system.bin of=$@ bs=512 count=200 seek=5 conv=notrunc # 5扇区开始写200个扇区，100k
# 分区
	sfdisk $@ < $(SRC)/utils/LightOS.sfdisk




# 创建 grub 引导的系统 iso 镜像文件
$(BUILD)/kernel.iso: $(BUILD)/kernel.bin $(SRC)/utils/grub.cfg
	rm -f $(BUILD)/kernel.iso
	grub-file --is-x86-multiboot2 $< # 检查 iso 是否合法
	mkdir -p $(BUILD)/iso/boot/grub
	cp $< $(BUILD)/iso/boot
	cp $(SRC)/utils/grub.cfg $(BUILD)/iso/boot/grub
	grub-mkrescue -o $@ $(BUILD)/iso

$(BUILD)/slave.img:
	yes | bximage -q -hd=32 -func=create -sectsize=512 -imgmode=flat $@
	sfdisk $@ < $(SRC)/utils/slave.sfdisk

MNT_PATH:=/mnt/LightOS_FS


.PHONY: fs
fs: $(BUILD)/LightOS.img $(BUILD)/slave.img
# 创建 mount 文件夹
	sudo mkdir -p $(MNT_PATH)

# 挂载设备并获取设备名称
	$(eval LOOP_DEV := $(shell sudo losetup --partscan -f $(BUILD)/LightOS.img --show))
	@echo "Create master loop dev: ${LOOP_DEV}"
# 创建 minix 文件系统，第一版，文件名长度14字节
	sudo mkfs.minix -1 -n 14 $(LOOP_DEV)p1
# 挂载文件系统
	sudo mount ${LOOP_DEV}p1 $(MNT_PATH)
# 切换所有者
	sudo chown ${USER} $(MNT_PATH)
# 创建目录
	mkdir -p $(MNT_PATH)/home
	mkdir -p $(MNT_PATH)/d1/d2/d3/d4
#创建文件
	echo "hello LightOS, from root directory!" > $(MNT_PATH)/hello.txt
	echo "hello LightOS, from root directory!" > $(MNT_PATH)/home/hello.txt
# 卸载文件系统与设备
	sudo umount $(MNT_PATH)
	sudo losetup -d ${LOOP_DEV}

# slave 同理
	$(eval LOOP_DEV := $(shell sudo losetup --partscan -f $(BUILD)/slave.img --show))
	@echo "Create slave loop dev: ${LOOP_DEV}"
	sudo mkfs.minix -1 -n 14 $(LOOP_DEV)p1
	sudo mount ${LOOP_DEV}p1 $(MNT_PATH)
	sudo chown ${USER} $(MNT_PATH)
	echo "hello LightOS, from slave root directory!" > $(MNT_PATH)/hello.txt
	sudo umount $(MNT_PATH)
	sudo losetup -d ${LOOP_DEV}

# 检查文件系统
.PHONY: mount1
mount1: $(BUILD)/LightOS.img
	$(eval LOOP_DEV := $(shell sudo losetup --partscan -f $(BUILD)/LightOS.img --show))
	@echo "Create master loop dev: ${LOOP_DEV}"
	sudo mount ${LOOP_DEV}p1 $(MNT_PATH)
	sudo chown ${USER} $(MNT_PATH)

.PHONY: mount2
mount2: $(BUILD)/slave.img
	$(eval LOOP_DEV := $(shell sudo losetup --partscan -f $(BUILD)/slave.img --show))
	@echo "Create slave loop dev: ${LOOP_DEV}"
	sudo mount ${LOOP_DEV}p1 $(MNT_PATH)
	sudo chown ${USER} $(MNT_PATH)

.PHONY: umount
umount:
	@if [ -z "$(DEV)" ]; then \
		echo "Error: DEV parameter is missing. Usage: make umount DEV=/dev/loop3"; \
		exit 1; \
	fi
	@echo "Unmounting device: $(DEV)"
	sudo umount $(DEV)p1
	sudo losetup -d $(DEV)


IMAGES:= $(BUILD)/LightOS.img $(BUILD)/slave.img fs