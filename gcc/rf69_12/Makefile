# gcc Makefile for LPC812
# based on original file by Kamal Mostafa <kamal@whence.com>

OBJS = main.o rf69_12.o ../base/spi.o ../base/uart.o \
			../base/printf.o ../base/printf-retarget.o ../base/iap_driver.o \
			../base/system_LPC8xx.o ../base/gcc_startup_lpc8xx.o

include ../base/rules.mk

# variable overrides and defaults can be placed in these optional files, e.g.
# 	CFLAGS += -DLPC_MAX=0
# 	TTY= /dev/tty.SLAB_USBtoUART

-include ../base/local.mk
-include local.mk
