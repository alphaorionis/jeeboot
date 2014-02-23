ng = angular.module 'myApp', [
  'ui.router'
  'ngAnimate'
  'mm.foundation'
]

ng.run (jeebus) ->
  jeebus.connect 'jeeboot'
