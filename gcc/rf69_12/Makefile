# gcc Makefile for LPC812
# based on original file by Kamal Mostafa <kamal@whence.com>

CROSS = arm-none-eabi-
CPU = -mthumb -mcpu=cortex-m0plus
WARN = -Wall
STD = -std=gnu99
LPCX = /Applications/lpcxpresso_6.1.0_164/lpcxpresso/bin

CC = ${CROSS}gcc
LD = ${CROSS}ld
OBJCOPY = ${CROSS}objcopy
SIZE = ${CROSS}size

LIBGCC = ${shell ${CC} ${CFLAGS} --print-libgcc-file-name}

CFLAGS += $(CPU) $(WARN) $(STD) -MMD -I../base -DIRQ_DISABLE \
			-Os -ffunction-sections -fno-builtin -ggdb
LDFLAGS += --gc-sections -Map=firmware.map --cref --library-path=../base

OBJS = main.o rf69_12.o ../base/spi.o ../base/uart.o \
			../base/printf.o ../base/printf-retarget.o ../base/iap_driver.o \
			../base/system_LPC8xx.o ../base/gcc_startup_lpc8xx.o

OS = $(shell uname)

ifeq ($(OS), Linux)
TTY = /dev/ttyUSB*
endif

ifeq ($(OS), Darwin)
TTY = /dev/tty.usbserial-*
endif

all: firmware.bin

firmware.elf: ../base/LPC812-boot.ld $(OBJS)
	@$(LD) -o $@ $(LDFLAGS) -T ../base/LPC812-boot.ld $(OBJS) $(LIBGCC)
	$(SIZE) $@

clean:
	rm -f *.o *.d # firmware.{elf,hex,bin,map}
	make -C ../debug clean

# this works for the LPC812 MAX board
flash: firmware.bin
	cp firmware.bin /Volumes/MBED/

dfu:
	$(LPCX)/dfu-util -d 0x471:0xdf55 -c 0 -t 2048 -R -D $(LPCX)/LPCXpressoWIN.enc

lpcx: firmware.elf
	$(LPCX)/crt_emu_cm3_gen -vendor=NXP -pLPC812 -wire=winUSB \
	     -flash-load-exec firmware.elf
			 
isp:
	lpc21isp firmware.hex $(TTY) 115200 12000

.PHONY: dfu lpcx
  
%.bin:%.elf
	@$(OBJCOPY) --strip-unneeded -O ihex firmware.elf firmware.hex
	@$(OBJCOPY) --strip-unneeded -O binary firmware.elf firmware.bin

-include $(OBJS:.o=.d)
