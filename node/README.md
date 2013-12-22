JeeBoot server based on Node.js and CoffeeScript.

To set up:

    cd node
    npm install

To run:

    node .

Currently uses a config file in `app/config.coffee` - the format and contents
of this is bound to change (a lot) as I figure out what is concvenient.

Changes to the config file cause a reload and work while the server is running.

Firmware files need to be placed in a directory called `firmware/` and can be
listed in the config file. Only Intel hex file format is currently recognised.

Once everything is up and running, firmware updates are done by updating the
firware file and making the remote node re-enter its boot looder in some way.
Firmware file changes are not yet picked up automatically by the server, it
currently needs to be restarted - obviously something which needs to be fixed.
