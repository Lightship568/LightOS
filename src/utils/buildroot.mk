
BUILD_ROOT_PATH ?= $(BUILD)/buildroot

# buildroot(*.asm)
$(BUILD_ROOT_PATH)/%.o: $(SRC)/buildroot/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f elf32 $(DEBUG) $< -o $@

# buildroot (*.c)
$(BUILD_ROOT_PATH)/%.o: $(SRC)/buildroot/%.c
	$(shell mkdir -p $(dir $@))
	gcc $(CFLAGS) $(DEBUG) $(INCLUDE) -c $< -o $@

# buildroot (.out): libc.o + *.o => *.out
$(BUILD_ROOT_PATH)/%.out:	\
	$(BUILD_ROOT_PATH)/%.o	\
	$(BUILD)/lib/libc.o

	ld -m elf_i386 -static $^ -o $@

BIN_APPS:= \
	$(BUILD_ROOT_PATH)/env.out	\
	$(BUILD_ROOT_PATH)/echo.out	\
	$(BUILD_ROOT_PATH)/cat.out	\
	$(BUILD_ROOT_PATH)/ls.out	\
	

.PHONY: buildroot
buildroot: \
	$(BUILD)/LightOS.img		\
	$(BUILD)/slave.img			\
	$(BIN_APPS)


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
	mkdir -p $(MNT_PATH)/bin
	mkdir -p $(MNT_PATH)/dev
	mkdir -p $(MNT_PATH)/mnt
#创建文件
	echo "hello LightOS, from root directory!" > $(MNT_PATH)/hello.txt

# Here to operate!
	for app in $(BIN_APPS); \
	do \
		cp $$app $(MNT_PATH)/bin; \
	done

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