HOST      = bbb
SERIALDEV = net:$(HOST):2000
include /home/arduino/arduino-0-5.mk

run: upload size
	nc $(HOST) 2001
