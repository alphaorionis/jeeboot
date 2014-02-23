ng = angular.module 'myApp'

ng.config ($stateProvider) ->
  $stateProvider.state 'home',
    url: '/'
    templateUrl: 'jeeboot.html'
    controller: 'JeeBootCtrl'

ng.controller 'JeeBootCtrl', ($scope, $timeout, jeebus) ->
  # TODO rewrite these example to use the "hm" service i.s.o. "jeebus"

  # TODO this delay seems to be required to avoid an error with WS setup - why?
  $timeout ->
    $scope.hwid = jeebus.attach '/jeeboot/hwid/'
    $scope.$on '$destroy', -> jeebus.detach '/jeeboot/hwid/'
    $scope.swid = jeebus.attach '/jeeboot/swid/'
    $scope.$on '$destroy', -> jeebus.detach '/jeeboot/swid/'
  , 100

  $scope.onFileDrop = (x) ->
    lastId = Object.keys($scope.swid).sort().pop() | 0
    lastId = 999  if lastId < 999
    for f in x
      r = new FileReader()
      r.onload = (e) ->
        jeebus.store "/jeeboot/swid/#{++lastId}", file: f.name
        jeebus.rpc 'store', "firmware/#{f.name}", e.target.result
      r.readAsText f

  $scope.fwDel = (swid) ->
    jeebus.store "/jeeboot/swid/#{swid}"
    jeebus.rpc 'store', "firmware/#{$scope.swid[swid].file}"

  $scope.hwDel = (hwid) ->
    jeebus.store "/jeeboot/hwid/#{hwid}"

  $scope.hwSave = (id, field, value) ->
    row = $scope.hwid[id]
    row[field] = value | 0 # TODO hard-coded int conversion
    jeebus.store "/jeeboot/hwid/#{id}", row

# see http://docs.angularjs.org/guide/forms
ng.directive 'contenteditable', ($parse) ->
  restrict: 'A'
  link: (scope, elm, attr) ->
    if attr.onBlur
      elm.on 'blur', ->
        scope.$apply ->
          fn = $parse attr.onBlur
          fn scope, $value: elm.text()

# see also github.com/danialfarid/angular-file-upload
ng.directive 'onFileDrop', ($parse) ->
  restrict: 'A'
  link: (scope, elem, attr) ->

    elem[0].addEventListener 'dragover', (evt) ->
      evt.stopPropagation()
      evt.preventDefault()
      elem.addClass 'dragActive'

    elem[0].addEventListener 'dragleave', (evt) ->
      elem.removeClass 'dragActive'

    elem[0].addEventListener 'drop', (evt) ->
      evt.stopPropagation()
      evt.preventDefault()
      elem.removeClass 'dragActive'

      fn = $parse attr.onFileDrop
      fl = (x for x in evt.dataTransfer.files)
      fn scope, $files: fl, $event: evt
