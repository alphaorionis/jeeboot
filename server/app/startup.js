// Generated by CoffeeScript 1.7.1
(function() {
  var ng;

  ng = angular.module('myApp', ['ui.router', 'ngAnimate', 'mm.foundation']);

  ng.value('appInfo', {
    name: 'JeeBus',
    version: '0.3.0',
    home: 'https://github.com/jcw/jeebus'
  });

  ng.run(function(jeebus) {
    return jeebus.connect('jeebus');
  });

  ng.run(function($rootScope, appInfo) {
    $rootScope.appInfo = appInfo;
    return $rootScope.shared = {};
  });

  ng.directive('appVersion', function(appInfo) {
    return function(scope, elm, attrs) {
      return elm.text(appInfo.version);
    };
  });

}).call(this);

//# sourceMappingURL=startup.map
