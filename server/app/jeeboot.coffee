ng = angular.module 'myApp'

ng.config ($stateProvider, navbarProvider) ->
  $stateProvider.state 'home',
    url: '/'
    templateUrl: 'jeeboot.html'
    controller: 'JeeBootCtrl'

ng.controller 'JeeBootCtrl', ($scope, jeebus) ->
  # TODO rewrite these example to use the "hm" service i.s.o. "jeebus"
