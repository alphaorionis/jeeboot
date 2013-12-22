stream = require 'stream'
fs = require 'fs'
config = require '../config'

FIRMWARE_DIR = './firmware'

nodeConfigs = {}
codeCache = {}

readIntelHexFile = (name) ->
  hex = fs.readFileSync "#{FIRMWARE_DIR}/#{name}", 'utf8'
  chunks = []
  for line in hex.split '\n'
    if line[0] is ':'
      line = line.slice(0, -1)  if line[line.length-1] is '\r'
      buf = new Buffer(line.slice(1), 'hex')
      if buf[3] is 0
        chunks.push buf.slice 4, -1
  Buffer.concat chunks

padToBinaryMultiple = (buf, count) ->
  remain = buf.length % count
  if remain
    pad = new Buffer(count - remain)
    pad.fill 0xFF
    Buffer.concat [buf, pad]
  else
    buf

crcTable = [
  0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401,
  0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400
]

calculateCrc = (buf) ->
  crc = 0xFFFF
  for data in buf
    crc = (crc >> 4) ^ crcTable[crc & 0xF] ^ crcTable[data & 0xF]
    crc = (crc >> 4) ^ crcTable[crc & 0xF] ^ crcTable[data >> 4]
  crc

getCode = (tgn) ->
  nodeName = "#{config.types[tgn.type]},#{tgn.group},#{tgn.nodeId}"
  # loop through all entries and remember only the last one
  for swId,fileName of config.nodes[nodeName]
    swId |= 0 # convert to int
  code = padToBinaryMultiple readIntelHexFile(fileName), 64
  swCheck = calculateCrc code
  swSize = code.length >> 4
  nodeConfigs["rf12-868,#{tgn.group},#{tgn.nodeId}"] = swId
  codeCache[swId] = {swId,swCheck,swSize,code}
  console.log 'glc', nodeName, fileName, swId, code.length, swSize, swCheck

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
          reply = @upgradeRequest msg, data.type
        when 5
          reply = @downloadRequest msg
        else
          console.log 'bad message:', data
      if reply
        cmd = reply.toJSON().toString() + ',0s'
        console.log ' ->', reply.length
        @push cmd
    else
      # console.log data
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

  upgradeRequest: (msg, cfg) ->
    info =
      type: msg.readUInt16LE 0
      swId: msg.readUInt16LE 2
      swSize: msg.readUInt16LE 4
      swCheck: msg.readUInt16LE 6
      config: cfg
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
    #console.log 'dlr', info.swId, info.swIndex, info.swId ^ info.swIndex
    reply.writeUInt16LE info.swId ^ info.swIndex, 0
    pos = info.swIndex * 64
    code = codeCache[info.swId].code
    #code.copy reply, 2, pos, pos+64
    #console.log code.slice(pos, pos+64).toString 'hex'
    for i in [0..63]
      reply[2+i] = (code[pos+i] ^ (211*i)) & 0xFF # add data whitening
    reply

  doPairing: (info) ->
    console.log 'pairing', info
    info[k] = v  for k,v of config.hwIds[info.hwId]
    getCode info

  doUpgrade: (info) ->
    swId = nodeConfigs[info.config]
    console.log 'upgrade', info, swId
    info[k] = v  for k,v of codeCache[swId]

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
