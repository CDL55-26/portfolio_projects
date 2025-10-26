GCC     = riscv64-unknown-elf-gcc
OBJDUMP = riscv64-unknown-elf-objdump

CFLAGS  = -Wall -Wextra -g -O2 -ffreestanding \
          -march=rv64gc -mabi=lp64 -mcmodel=medany \
          -Iinclude
LDFLAGS = -nostdlib -nostartfiles -T linker.ld -Wl,-Map=build/kernel.map

SRC_DIRS := src src/arch src/drivers src/kernel #all directories to look in to compile
VPATH := $(SRC_DIRS) #fill in with src paths if make cant find

C_FILES   := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
ASM_FILES := $(wildcard *.S) $(wildcard src/arch/*.S)

C_OBJS   := $(patsubst %.c,build/%.o,$(notdir $(C_FILES)))
ASM_OBJS := $(patsubst %.S,build/%.o,$(notdir $(ASM_FILES)))
OBJS := $(C_OBJS) $(ASM_OBJS)

all: build build/kernel.elf build/kernel.asm

build:
	mkdir -p build

build/%.o: %.c
	$(GCC) $(CFLAGS) -c $< -o $@

build/%.o: %.S
	$(GCC) $(CFLAGS) -c $< -o $@

build/kernel.elf: $(OBJS) linker.ld
	$(GCC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) -lgcc

build/kernel.asm: build/kernel.elf
	$(OBJDUMP) -d $< > $@

run: build/kernel.elf
	qemu-system-riscv64 -machine virt -nographic -bios none -kernel $<

clean:
	rm -rf build

.PHONY: all run clean build
