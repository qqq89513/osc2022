$(info Executing in shell: $(SHELL))


BUILD_DIR = ./build/locals
OUTPUT_DIR = $(BUILD_DIR)/..
SRCS = $(wildcard *.c) $(wildcard ../api/src/*.c)
OBJS = $(addprefix $(BUILD_DIR)/, $(SRCS:.c=.o))
INCLUDES = -I ../api/inc/ -I ./
CCFLAGS = -Wall -O3 -nostartfiles -ffreestanding 
# CCFLAGS = -nostartfiles -ffreestanding -mthumb -Wall -fdump-rtl-expand -specs=nano.specs --specs=rdimon.specs   -Wl,--start-group -lgcc -lc -lm -lrdimon -Wl,--end-group
ASMFLAGS = $(CCFLAGS)
LDFLAGS = -T link.ld -O3 -nostartfiles -ffreestanding -specs=nosys.specs
# LDFLAGS = -T link.ld -nostartfiles -ffreestanding -nostartfiles -ffreestanding -mthumb -Wall -fdump-rtl-expand -specs=nano.specs --specs=rdimon.specs   -Wl,--start-group -lgcc -lc -lm -lrdimon -Wl,--end-group
TOOL_CHAIN_PREFIX = aarch64-none-elf
CC = $(TOOL_CHAIN_PREFIX)-gcc
LD = $(TOOL_CHAIN_PREFIX)-gcc
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
	cp $(OUTPUT_DIR)/kernel8.img /D/kernel8.img
