.PHONY: bochs
bochs: $(BUILD)/LightOS.img
	bochs -q -f ../bochs/bochsrc

# -s：监听1234端口，-S：GDB没有连接QEMU之前不执行
# -c 硬盘启动 -m 32M的内存大小

QEMU:= qemu-system-i386
QEMU+= -m 32M
QEMU+= -audiodev pa,id=hda,server=/run/user/1000/pulse/native
QEMU+= -machine pcspk-audiodev=hda
QEMU+= -rtc base=localtime

QEMU_DISK:= -boot c
QEMU_DISK+= -drive file=$(BUILD)/LightOS.img,if=ide,index=0,media=disk,format=raw 	# 主硬盘
QEMU_DISK+= -drive file=$(BUILD)/slave.img,if=ide,index=1,media=disk,format=raw 	# 从硬盘

QEMU_CDROM:= -boot d
QEMU_CDROM+= -drive file=$(BUILD)/kernel.iso,media=cdrom
# 测试指定一个IDE主盘
QEMU_CDROM+= -drive file=$(BUILD)/LightOS.img,if=ide,index=1,media=disk,format=raw 	# 主硬盘

QEMU_DEBUG:=-s -S

.PHONY: qemu
qemu: $(IMAGES)
	$(QEMU) $(QEMU_DISK)

.PHONY: qemug
qemug: $(IMAGES)
	$(QEMU) $(QEMU_DISK) $(QEMU_DEBUG)

# .PHONY: qemu-grub
# qemu-grub: $(BUILD)/kernel.iso
# 	$(QEMU) $(QEMU_CDROM)

# .PHONY: qemug-grub
# qemug-grub: $(BUILD)/kernel.iso
# 	$(QEMU) $(QEMU_CDROM) $(QEMU_DEBUG)

.PHONY: iso
iso: $(BUILD)/kernel.iso

# $(BUILD)/LightOS.vmdk: $(BUILD)/LightOS.img
# 	qemu-img convert -O vmdk $< $@
# .PHONY: vmdk
# vmdk: $(BUILD)/LightOS.vmdk


.PHONY: clean
clean:
	rm -rf $(BUILD)

test: $(BUILD)/LightOS.img