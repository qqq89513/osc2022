$(info Executing in shell: $(SHELL))


BUILD_DIR = ./build/locals
OUTPUT_DIR = $(BUILD_DIR)/..
BUILD_DIR_gdb = ./debug/locals
OUTPUT_DIR_gdb = $(BUILD_DIR_gdb)/..
SRCS = $(wildcard *.c) $(wildcard ../api/src/*.c)
SRCS_asm = $(wildcard *.S)
OBJS = $(addprefix $(BUILD_DIR)/, $(SRCS:.c=.o))
OBJS_asm = $(addprefix $(BUILD_DIR)/, $(SRCS_asm:.S=.o))
INCLUDES = -I ../api/inc/ -I ./
CCFLAGS = -Wall -nostartfiles -ffreestanding -nostdlib -static
# CCFLAGS = -nostartfiles -ffreestanding -mthumb -Wall -fdump-rtl-expand -specs=nano.specs --specs=rdimon.specs   -Wl,--start-group -lgcc -lc -lm -lrdimon -Wl,--end-group
ASMFLAGS = $(CCFLAGS)
LDFLAGS = -T link.ld -nostartfiles -ffreestanding -nostdlib -static
CCFLAGS_gdb = $(CCFLAGS) -ggdb
ASMFLAGS_gdb = $(ASMFLAGS) -ggdb
LDFLAGS_gdb = $(LDFLAGS) -ggdb
# LDFLAGS = -T link.ld -nostartfiles -ffreestanding -nostartfiles -ffreestanding -mthumb -Wall -fdump-rtl-expand -specs=nano.specs --specs=rdimon.specs   -Wl,--start-group -lgcc -lc -lm -lrdimon -Wl,--end-group
TOOL_CHAIN_PREFIX = aarch64-none-elf
CC = $(TOOL_CHAIN_PREFIX)-gcc
AS = $(TOOL_CHAIN_PREFIX)-as
LD = $(TOOL_CHAIN_PREFIX)-gcc
SIZE = $(TOOL_CHAIN_PREFIX)-size
OBJCOPY = $(TOOL_CHAIN_PREFIX)-objcopy

all: create_dir kernel8.img
build: all
b: build
c: clean
r: run
bg: build_gdb
rg: run_gdb

create_dir:
	@mkdir -p $(dir $(OBJS))
	@mkdir -p $(dir $(OBJS_asm))
	@echo OUTPUT_DIR=$(OUTPUT_DIR)

$(BUILD_DIR)/%.o: %.S
	$(CC) $(ASMFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	$(CC)  $(CCFLAGS) $(INCLUDES) -c $< -o $@

kernel8.img: $(OBJS_asm) $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS_asm) $(OBJS) -o $(OUTPUT_DIR)/kernel8.elf
	$(OBJCOPY) -O binary $(OUTPUT_DIR)/kernel8.elf $(OUTPUT_DIR)/kernel8.img
	$(SIZE) $(OUTPUT_DIR)/kernel8.elf

clean:
	rm build/ -rf
	rm debug/ -rf

run:
	qemu-system-aarch64 -M raspi3b -display none -serial null -serial stdio -initrd initramfs.cpio -dtb bcm2710-rpi-3-b-plus.dtb -kernel $(OUTPUT_DIR)/kernel8.img

gdb:
	aarch64-none-elf-gdb.exe --eval-command="target remote:1234" ./debug/kernel8.elf

run_gdb:
	qemu-system-aarch64 -M raspi3b -display none -serial null -serial stdio -initrd initramfs.cpio -dtb bcm2710-rpi-3-b-plus.dtb -kernel $(OUTPUT_DIR_gdb)/kernel8.img -gdb tcp::1234 -S --accel tcg,thread=single

build_gdb:
	make BUILD_DIR="$(BUILD_DIR_gdb)" ASMFLAGS="$(ASMFLAGS_gdb)" CCFLAGS="$(CCFLAGS_gdb)" LDFLAGS="$(LDFLAGS_gdb)" all --no-print-directory

build_el0: create_dir
	$(AS) -c user.asm -o $(BUILD_DIR)/user.o
	$(LD) $(LDFLAGS) $(BUILD_DIR)/user.o -o $(OUTPUT_DIR)/user.elf
	$(OBJCOPY) -O binary $(OUTPUT_DIR)/user.elf $(OUTPUT_DIR)/user.img
	$(SIZE) $(OUTPUT_DIR)/user.elf

flash:
	cp $(OUTPUT_DIR)/kernel8.img /D/kernel8.img
	cp initramfs.cpio /D/initramfs.cpio
	cp bcm2710-rpi-3-b-plus.dtb /D/bcm2710-rpi-3-b-plus.dtb
	cp config.txt /D/config.txt

flash_py:
	python ../lab2-bootloader/send_img.py $(OUTPUT_DIR)/kernel8.img
