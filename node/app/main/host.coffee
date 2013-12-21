stream = require 'stream'
fs = require 'fs'

class BootResponder extends stream.Transform
  constructor: ->
    super objectMode: true

  _transform: (data, encoding, done) ->
    if Buffer.isBuffer data.msg
      switch data.msg.length
        when 23
          type = data.msg.readUInt16LE 1
          group = data.msg[3]
          nodeId = data.msg[4]
          check = data.msg.readUInt16LE 5
          hwId = data.msg.toString 'hex', 7, 23
          console.log 'pairing', { type, group, nodeId, check, hwId }
        when 9
          type = data.msg.readUInt16LE 1
          swId = data.msg.readUInt16LE 3
          swSize = data.msg.readUInt16LE 5
          swCheck = data.msg.readUInt16LE 7
          console.log 'upgrade', { type, swId, swSize, swCheck }
        when 5
          swId = data.msg.readUInt16LE 1
          swIndex = data.msg.readUInt16LE 3
          console.log 'data', { swid, swIndex }
        else
          console.log 'bad message:', data
    done()

module.exports = (app, plugin) ->

  app.on 'running', ->
    Serial = @registry.interface.serial
    Parser = @registry.pipe.parser

    jeenode = new Serial('usb-A40115A2').on 'open', ->
      jeenode
        .pipe(new Parser)
        .pipe(new BootResponder)
        .pipe(jeenode)
