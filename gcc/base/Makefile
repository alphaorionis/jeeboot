# gcc Makefile for LPC812
# based on original file by Kamal Mostafa <kamal@whence.com>

CROSS = arm-none-eabi-
CPU = -mthumb -mcpu=cortex-m0plus

CC = ${CROSS}gcc
LD = ${CROSS}ld
OBJCOPY = ${CROSS}objcopy
SIZE = ${CROSS}size

CFLAGS += $(CPU) -MMD -I. -Os -ggdb
LDFLAGS += --gc-sections -Map=firmware.map --cref

OBJS = main.o system_LPC8xx.o gcc_startup_lpc8xx.o
       
all: firmware.bin

firmware.elf: LPC812-boot.ld $(OBJS)
	@$(LD) -o $@ $(LDFLAGS) -T LPC812-boot.ld $(OBJS)
	$(SIZE) $@

clean:
	rm -f *.o *.d # firmware.{elf,hex,bin,map}

# this works for the LPC812 MAX board
flash: firmware.bin
	cp firmware.bin /Volumes/MBED/
#	@diskutil eject `diskutil list | grep -B 2 MBED | grep dev`

%.bin:%.elf
	@$(OBJCOPY) --strip-unneeded -O ihex firmware.elf firmware.hex
	@$(OBJCOPY) --strip-unneeded -O binary firmware.elf firmware.bin

-include $(OBJS:.o=.d)
