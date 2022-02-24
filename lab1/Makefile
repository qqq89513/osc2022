$(info Executing in shell: $(SHELL))


BUILD_DIR = ./build/locals
OUTPUT_DIR = $(BUILD_DIR)/..
SRCS = $(wildcard *.c) $(wildcard ../api/src/*.c)
OBJS = $(addprefix $(BUILD_DIR)/, $(SRCS:.c=.o))
INCLUDES = -I ../api/inc/ -I ./
CCFLAGS = -Wall -O2 -nostartfiles -ffreestanding -nostdinc -nostdlib
ASMFLAGS = $(CCFLAGS)
LDFLAGS = -T link.ld --nostdlib
TOOL_CHAIN_PREFIX = aarch64-none-elf
CC = $(TOOL_CHAIN_PREFIX)-gcc
LD = $(TOOL_CHAIN_PREFIX)-ld
SIZE = $(TOOL_CHAIN_PREFIX)-size
OBJCOPY = $(TOOL_CHAIN_PREFIX)-objcopy

all: create_dir kernel8.img
build: all
b: build
c: clean
r: run

create_dir:
	@mkdir -p $(dir $(OBJS))

$(BUILD_DIR)/start.o: start.S
	$(CC) $(ASMFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	$(CC)  $(CCFLAGS) $(INCLUDES) -c $< -o $@

kernel8.img: $(BUILD_DIR)/start.o $(OBJS)
	$(LD) $(LDFLAGS) $(BUILD_DIR)/start.o $(OBJS) -o $(OUTPUT_DIR)/kernel8.elf
	$(OBJCOPY) -O binary $(OUTPUT_DIR)/kernel8.elf $(OUTPUT_DIR)/kernel8.img
	$(SIZE) $(OUTPUT_DIR)/kernel8.elf

clean:
	rm build/ -r

run:
	qemu-system-aarch64 -M raspi3b -display none -serial null -serial stdio -kernel $(OUTPUT_DIR)/kernel8.img

flash:
	cp $(OUTPUT_DIR)/kernel8.img /E/kernel8.img
