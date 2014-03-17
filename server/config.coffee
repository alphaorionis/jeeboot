#!/usr/bin/env coffee
# Generate hard-to-edit JSON config file from an easy-to-edit CoffeeScript one.
config = {}

config.swids =
  1001: '../firmware/blinkAvr1.hex'
  1002: '../firmware/blinkAvr2.hex'
  1003: '../firmware/blinkAvr3.hex'
	
config.hwids =
  '06300301c48461aeedb09351061900f5':
    board: 2, group: 212, node: 17, swid: 1001

# write configuration to file, but keep a backup of the original, just in case
fs = require('fs')
try fs.renameSync 'config.json', 'config-prev.json'
fs.writeFileSync 'config.json', JSON.stringify config, null, 4
