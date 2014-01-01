HOST      = bbb
SERIALDEV = net:$(HOST):2000
include ../arduino.mk

run: upload size
	nc $(HOST) 2001
