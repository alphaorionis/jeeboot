module.exports = (app) ->
  # specify which modules need to be loaded first, and in what order
  app.config.loadFirst = ['main']
  
  app.on 'setup', ->
    console.log "plugins: #{Object.keys(@config.plugin)}"
  
  app.on 'running', ->
    console.info "server listening on port :#{@config.port}"

  # debugging: log all app.emit calls
  # appEmit = app.emit
  # app.emit = (args...) ->
  #   console.log 'appEmit', args
  #   appEmit.apply @, args

  app.registry = {}

  app.register = (path, value) ->
    # console.log "registry.#{path} = #{typeof value}"
    segments = path.split '.'
    tail = segments.pop()
    lookup = @registry
    for x in segments
      lookup[x] ?= {}
      lookup = lookup[x]
    lookup[tail] = value

  # After having been called once on startup, this module turns itself into a
  # proxy for the global "app" object by adjusting its own exports object, so
  # "app = require '../launch'" becomes an easy way to get at the app object.
  module.exports = app
