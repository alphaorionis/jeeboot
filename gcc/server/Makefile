OBJS = main.o ../rf69_12/rf69_12.o ../base/spi.o ../base/uart.o \
       ../base/printf.o ../base/printf-retarget.o \
			 ../base/system_LPC8xx.o ../base/gcc_startup_lpc8xx.o
       
include ../base/rules.mk

CFLAGS += -DLPC_BES -I../rf69_12
CXXFLAGS += -DLPC_BES -I../rf69_12

-include ../base/local.mk
-include local.mk
