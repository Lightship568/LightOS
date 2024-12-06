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

.PHONY: images
images: $(BUILD)/LightOS.img $(BUILD)/slave.img