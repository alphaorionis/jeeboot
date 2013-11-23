Command-line code for the JeeBoot loader with some blink apps for testing.

The `rf69_12` folder contains a version for JeeBoot for ARM, using an RFM69
radio module in "RF12 compatibility mode", i.e. expecting an RF12-based boot
server on the other side, listening to group 212 (the fixed pairing group).

To build JeeBoot for EA's LPC MAX or NXP's LPC812 board, you must have the
gcc-arm-embedded toolchain installed and in your path. Build and upload using:

		cd rf69_12
		make flash    // for the LPC MAX board, or...
		make isp      // for the LPC812 board (needs the lpc21isp utility)

Note: you need to set LPC_MAX to 0 in `main.c` for NXP's LPC812 board.
