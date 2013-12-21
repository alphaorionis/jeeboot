stream = require 'stream'
fs = require 'fs'

class BootResponder extends stream.Transform
  constructor: ->
    super objectMode: true

  _transform: (data, encoding, done) ->
    if Buffer.isBuffer data.msg
      msg = data.msg.slice 1
      switch data.msg.length
        when 23
          reply = @pairingRequest msg
        when 9
          reply = @upgradeRequest msg
        when 5
          reply = @downloadRequest msg
        else
          console.log 'bad message:', data
      if reply
        console.log ' ->', reply
    done()

  pairingRequest: (msg) ->
    info =
      type: msg.readUInt16LE 0
      group: msg[2]
      nodeId: msg[3]
      check: msg.readUInt16LE 4
      hwId: msg.toString 'hex', 6, 22
    @doPairing info
    reply = new Buffer(20)
    reply.writeUInt16LE info.type, 0
    reply.writeUInt8 info.group, 2
    reply.writeUInt8 info.nodeId, 3
    if info.shKey
      new Buffer(info.shKey, 'hex').copy(reply, 4)
    else
      reply.fill 0, 4
    reply

  upgradeRequest: (msg) ->
    info =
      type: msg.readUInt16LE 0
      swId: msg.readUInt16LE 2
      swSize: msg.readUInt16LE 4
      swCheck: msg.readUInt16LE 6
    @doUpgrade info
    reply = new Buffer(8)
    reply.writeUInt16LE info.type, 0
    reply.writeUInt16LE info.swId, 2
    reply.writeUInt16LE info.swSize, 4
    reply.writeUInt16LE info.swCheck, 6
    reply

  downloadRequest: (msg) ->
    info =
      swId: msg.readUInt16LE 0
      swIndex: msg.readUInt16LE 2
    @doDownload info
    reply = new Buffer(66)
    reply.writeUInt16LE info.swId ^ info.swIndex, 0
    reply

  doPairing: (info) ->
    console.log 'pairing', info

  doUpgrade: (info) ->
    console.log 'upgrade', info

  doDownload: (info) ->
    console.log 'download', info

module.exports = (app, plugin) ->

  app.on 'running', ->
    Serial = @registry.interface.serial
    Parser = @registry.pipe.parser

    jeenode = new Serial('usb-A40115A2').on 'open', ->
      jeenode
        .pipe(new Parser)
        .pipe(new BootResponder)
        .pipe(jeenode)
