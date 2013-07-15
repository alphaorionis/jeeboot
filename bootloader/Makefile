# Simplified version of "Makefile for ATmegaBOOT" by Erik Lins, 18.7.2005
# Used here to compile and upload JeeBoot -jcw, 2012-11-01
#
# To make bootloader .hex file:		make atmega328
# To burn bootloader .hex file:		make atmega328_isp

PROGRAM = ota_boot
MCU_TARGET = atmega328
TARGET = atmega328
AVR_FREQ = 16000000L
LDSECTION = --section-start=.text=0x7800

# enter the parameters for the avrdude isp tool
# ISP_ARGS = -c stk500v2 -P usb -b 115200
ISP_ARGS = -c stk500v1 -P /dev/tty.usbserial-A600dW8R -b 9600

ISPFUSES = avrdude $(ISP_ARGS) -p $(MCU_TARGET) \
 -e -u -U lock:w:0x3f:m -qq \
 -U efuse:w:0x$(EFUSE):m -U hfuse:w:0x$(HFUSE):m -U lfuse:w:0x$(LFUSE):m \
 -U lock:w:0x0f:m
ISPFLASH = avrdude -c $(ISP_ARGS) -p $(MCU_TARGET) \
 -V -q -s -U flash:w:$(PROGRAM)_$(TARGET).hex

OBJ = $(PROGRAM).o
OPTIMIZE = -Os -fno-inline-small-functions -fno-split-wide-types -mshort-calls

DEFS = 
LIBS =

CC      = avr-gcc
OBJCOPY = avr-objcopy
OBJDUMP = avr-objdump

# Override is only needed by avr-lib build system.

override CFLAGS = -g -Wall $(OPTIMIZE) -mmcu=$(MCU_TARGET) -DF_CPU=$(AVR_FREQ) \
 -std=gnu99 $(DEFS)
override LDFLAGS = -Wl,$(LDSECTION) -Wl,--relax -nostartfiles

atmega328: MCU_TARGET = atmega328p
atmega328: TARGET = atmega328
atmega328: AVR_FREQ = 16000000L
atmega328: LDSECTION = --section-start=.text=0x7000
atmega328: $(PROGRAM)_atmega328.hex $(PROGRAM)_atmega328.lst

# HFUSE = DA - 2048 byte boot, D8 - 4096 byte boot
# LFUSE = 4E - Low power resonator (16MHz), fast start, clkdiv/8
# EFUSE = 06 - 1.8V brownout (hence the clkdiv/8 requirement)
atmega328_isp: atmega328
atmega328_isp: HFUSE = D8
atmega328_isp: LFUSE = 4E
atmega328_isp: EFUSE = 06
atmega328_isp: isp

attiny84: MCU_TARGET = t84
attiny84: TARGET = attiny84
attiny84: AVR_FREQ = 800000L
attiny84: LDSECTION = --section-start=.text=0x1800
attiny84: $(PROGRAM)_attiny84.hex $(PROGRAM)_attiny84.lst

isp: $(TARGET)
	$(ISPFUSES)
	$(ISPFLASH)

%.elf: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)
	avr-size $@

clean:
	rm -rf *.o *.elf *.lst *.map *.sym *.lss *.eep *.srec *.bin *.hex

%.lst: %.elf
	$(OBJDUMP) -h -S $< > $@

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@
%.srec: %.elf
	$(OBJCOPY) -j .text -j .data -O srec $< $@
%.bin: %.elf
	$(OBJCOPY) -j .text -j .data -O binary $< $@
