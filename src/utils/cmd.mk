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
# 串口
# vc 的显示在 qemu tab 的 com base 中
# QEMU+= -chardev vc,mux=on,id=com1 # 字符设备 1
QEMU+= -chardev stdio,mux=on,id=com1 # 字符设备 1 (qemu terminal)
# QEMU+= -chardev vc,mux=on,id=com2 # 字符设备 2
QEMU+= -chardev udp,id=com2,port=6666,ipv4=on # 字符设备 2 (nc -ulp 6666)
QEMU+= -serial chardev:com1 # 串口 1
QEMU+= -serial chardev:com2 # 串口 2

QEMU_DISK_BOOT:= -boot c
QEMU_DISK_BOOT+= -drive file=$(BUILD)/LightOS.img,if=ide,index=0,media=disk,format=raw 	# 主硬盘
QEMU_DISK_BOOT+= -drive file=$(BUILD)/slave.img,if=ide,index=1,media=disk,format=raw 	# 从硬盘

QEMU_CDROM:= -boot d
QEMU_CDROM+= -drive file=$(BUILD)/kernel.iso,media=cdrom
# 测试指定一个IDE主盘
QEMU_CDROM+= -drive file=$(BUILD)/LightOS.img,if=ide,index=1,media=disk,format=raw 	# 主硬盘

QEMU_DEBUG:=-s -S

# buildroot 依赖 images，仅编译 images 会重置清空 buildroot
ALL:= images buildroot

.PHONY: qemu
qemu: $(ALL)
	$(QEMU) $(QEMU_DISK_BOOT)

.PHONY: qemug
qemug: $(ALL)
	$(QEMU) $(QEMU_DISK_BOOT) $(QEMU_DEBUG)

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