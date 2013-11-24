CROSS = arm-none-eabi-
CPU = -mthumb -mcpu=cortex-m0plus
WARN = -Wall
STD = -std=gnu99

CC = $(CROSS)gcc
LD = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy
SIZE = $(CROSS)size

CFLAGS += $(CPU) $(WARN) $(STD) -MMD -I../base -DIRQ_DISABLE \
          -Os -ffunction-sections -fno-builtin -ggdb

LDFLAGS += --gc-sections -Map=firmware.map --cref --library-path=../base
LIBGCC = $(shell $(CC) $(CFLAGS) --print-libgcc-file-name)

OS := $(shell uname)

ifeq ($(OS), Linux)
TTY = /dev/ttyUSB*
endif

ifeq ($(OS), Darwin)
TTY = /dev/tty.usbserial-*
LPCX = /Applications/lpcxpresso_6.1.0_164/lpcxpresso/bin
endif

all: firmware.bin

firmware.elf: ../base/LPC812-boot.ld $(OBJS)
	@$(LD) -o $@ $(LDFLAGS) -T ../base/LPC812-boot.ld $(OBJS) $(LIBGCC)
	$(SIZE) $@

clean:
	rm -f *.o *.d # firmware.{elf,hex,bin,map}
	make -C ../base clean

# this works for EA's LPC812 MAX board
flash: firmware.bin
	cp firmware.bin /Volumes/MBED/

# these two work with NXP's LPC812 board, using JLink
dfu:
	$(LPCX)/dfu-util -d 0x471:0xdf55 -c 0 -t 2048 -R -D $(LPCX)/LPCXpressoWIN.enc
lpcx: firmware.elf
	$(LPCX)/crt_emu_cm3_gen -vendor=NXP -pLPC812 -wire=winUSB \
	     -flash-load-exec firmware.elf
			 
# this works with NXP's LPC812 board, using serial ISP
isp:
	lpc21isp firmware.hex $(TTY) 115200 12000

.PHONY: all clean flash dfu lpcx isp
  
%.bin:%.elf
	@$(OBJCOPY) --strip-unneeded -O ihex firmware.elf firmware.hex
	@$(OBJCOPY) --strip-unneeded -O binary firmware.elf firmware.bin
