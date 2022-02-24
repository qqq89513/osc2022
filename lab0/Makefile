$(info $(SHELL))

SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
CFLAGS = -Wall -O2 -ffreestanding -nostdinc -nostdlib -nostartfiles
TOOL_CHAIN_PREFIX = aarch64-none-elf
CC = $(TOOL_CHAIN_PREFIX)-gcc
LD = $(TOOL_CHAIN_PREFIX)-ld
SIZE = $(TOOL_CHAIN_PREFIX)-size
OBJCOPY = $(TOOL_CHAIN_PREFIX)-objcopy

all: kernel8.img

start.o: start.S
	$(CC) $(CFLAGS) -c start.S -o start.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel8.img: start.o $(OBJS)
	$(LD) -nostdlib start.o $(OBJS) -T link.ld -o kernel8.elf
	$(SIZE) kernel8.elf
	$(OBJCOPY) -O binary kernel8.elf kernel8.img

clean:
	rm kernel8.elf *.o kernel8.img >/dev/null 2>/dev/null || true

run:
	qemu-system-aarch64 -M raspi3 -kernel kernel8.img -serial null -serial stdio
