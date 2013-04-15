# install the OTA boot loader into an ATmega328 using ISP

tty="/dev/tty.usbserial-A600dW8R"
hex="ota_boot_atmega328.hex"
set -x
avrdude -p m328p -c stk500v1 -P $tty -b 9600 \
  -U hfuse:w:0xDA:m -U lfuse:w:0x4E:m -U efuse:w:0x06:m
sleep 0.5
avrdude -p m328p -c stk500v1 -P $tty -b 9600 -U flash:w:$hex
