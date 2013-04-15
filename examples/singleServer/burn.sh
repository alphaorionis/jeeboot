# send test sketch to the singleServer ISP emulator for OTA to remote nodes
# default is test1, use arg 2 or 3 to send test2 or test3 instead

tty="/dev/tty.usbserial-A600K04C"
case "${1-1}" in
  1|2|3)  hex="../testServer/testBlink${1-1}.cpp.hex" ;;
  *)      hex="$1" ;;
esac
set -x
avrdude -p m328p -c stk500v1 -P $tty -b 57600 -U flash:w:$hex
