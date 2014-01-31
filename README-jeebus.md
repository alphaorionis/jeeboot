JeeBoot server based on JeeBus, written in Go.

To set up, you need JeeBus running on the same machine, then:

    cd jeebus
    go run main.go boots usb-A900ad5m

Oh, and you currently need to have a serial port listener running, as follows:

    jb serial /dev/tty.usbserial-A900ad5m

Replace both names of the interface by yours (e.g. /dev/ttyUSB0 on Linux).

Currently uses a config file in `jeebus/config.json` - the format and contents
of this is bound to change (a lot) as I figure out what is convenient.

Changes to the config file cause a reload and work while the server is running.

Firmware files need to be placed in a directory called `firmware/` and can be
listed in the config file. Only Intel hex file format is currently recognised.

Once everything is up and running, firmware updates are done by updating the
firware file and making the remote node re-enter its boot looder in some way.
Firmware file changes are not picked up automatically by the `boots` process,
it currently has to be restarted - obviously something which needs to be fixed.

To debug errors, replace `log.Fatal` by `log.Panic` in the `check()` function.
