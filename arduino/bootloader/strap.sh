# install the OTA boot loader into an ATmega328 using ISP

tty="/dev/tty.usbserial-A600dVBa"
hex="ota_boot_atmega328.hex"
set -x
avrdude -V -p m328p -c stk500v1 -P $tty -b 19200 \
  -U hfuse:w:0xD8:m -U lfuse:w:0x4E:m -U efuse:w:0x06:m -U flash:w:$hex
