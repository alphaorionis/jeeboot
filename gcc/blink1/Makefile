# gcc Makefile for LPC812
# based on original file by Kamal Mostafa <kamal@whence.com>

CROSS = arm-none-eabi-
CPU = -mthumb -mcpu=cortex-m0plus

CC = ${CROSS}gcc
LD = ${CROSS}ld
OBJCOPY = ${CROSS}objcopy
SIZE = ${CROSS}size

LIBGCC = ${shell ${CC} ${CFLAGS} --print-libgcc-file-name}

CFLAGS += $(CPU) -MMD -I../base -Os -ffunction-sections -ggdb
LDFLAGS += --gc-sections -Map=firmware.map --cref --library-path=../base

OBJS = main.o ../base/system_LPC8xx.o ../base/gcc_startup_lpc8xx.o
       
all: firmware.bin

firmware.elf: ../base/LPC812-himem.ld $(OBJS)
	@$(LD) -o $@ $(LDFLAGS) -T ../base/LPC812-himem.ld $(OBJS) $(LIBGCC)
	$(SIZE) $@

clean:
	rm -f *.o *.d # firmware.{elf,hex,bin,map}
	make -C ../base clean

# this works for the LPC812 MAX board
flash: firmware.bin
	cp firmware.bin /Volumes/MBED/
#	@diskutil eject `diskutil list | grep -B 2 MBED | grep dev`

%.bin:%.elf
	@$(OBJCOPY) --strip-unneeded -O ihex firmware.elf firmware.hex
	@$(OBJCOPY) --strip-unneeded -O binary firmware.elf firmware.bin

-include $(OBJS:.o=.d)
