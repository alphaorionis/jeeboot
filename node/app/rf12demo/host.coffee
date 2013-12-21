stream = require 'stream'
serialport = require 'serialport'

class Serial extends serialport.SerialPort
  constructor: (dev, baudrate = 57600) ->
    # support some platform-specific shorthands
    switch process.platform
      when 'darwin' then port = dev.replace /^usb-/, '/dev/tty.usbserial-'
      when 'linux' then port = dev.replace /^tty/, '/dev/tty'
      else port = dev
    parser = lineEventParser dev
    super port, { baudrate, parser }

lineEventParser = (dev) ->
  origParser = serialport.parsers.readline /\r?\n/
  (emitter, buffer) ->
    emit = (type, part) ->
      if type is 'data' # probably always true
        part = { dev: dev, msg: part, time: Date.now() }
      emitter.emit type, part
    origParser { emit }, buffer

class Parser extends stream.Transform
  constructor: ->
    super objectMode: true
    @config = null

  _transform: (data, encoding, done) ->
    msg = data.msg
    if msg.length < 300
      tokens = msg.split ' '
      if tokens.shift() is 'OK'
        nodeId = tokens[0] & 0x1F
        prefix = if @config then "#{@config.band},#{@config.group}," else ''
        data.type = "rf12-#{prefix}#{nodeId}"
        data.msg = Buffer(tokens)
        @push data
      else if match = /^ \w i(\d+)\*? g(\d+) @ (\d\d\d) MHz/.exec msg
        @config = { recvid: +match[1], group: +match[2], band: +match[3] }
        console.info 'RF12 config:', msg
      else
        # unrecognized input, usually a "?" msg
        data.type = '?'
        @push data
    done()

module.exports = (app, plugin) ->
  app.register 'interface.serial', Serial
  app.register 'pipe.parser', Parser
