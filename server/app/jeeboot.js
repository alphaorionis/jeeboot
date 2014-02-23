(function() {
  var ng;

  ng = angular.module('myApp');

  ng.config(function($stateProvider, navbarProvider) {
    return $stateProvider.state('home', {
      url: '/',
      templateUrl: 'jeeboot.html',
      controller: 'JeeBootCtrl'
    });
  });

  ng.controller('JeeBootCtrl', function($scope, jeebus) {});

}).call(this);

//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoiIiwic291cmNlUm9vdCI6IiIsInNvdXJjZXMiOlsiamVlYm9vdC5jb2ZmZWUiXSwibmFtZXMiOltdLCJtYXBwaW5ncyI6IkFBQUE7QUFBQSxNQUFBLEVBQUE7O0FBQUEsRUFBQSxFQUFBLEdBQUssT0FBTyxDQUFDLE1BQVIsQ0FBZSxPQUFmLENBQUwsQ0FBQTs7QUFBQSxFQUVBLEVBQUUsQ0FBQyxNQUFILENBQVUsU0FBQyxjQUFELEVBQWlCLGNBQWpCLEdBQUE7V0FDUixjQUFjLENBQUMsS0FBZixDQUFxQixNQUFyQixFQUNFO0FBQUEsTUFBQSxHQUFBLEVBQUssR0FBTDtBQUFBLE1BQ0EsV0FBQSxFQUFhLGNBRGI7QUFBQSxNQUVBLFVBQUEsRUFBWSxhQUZaO0tBREYsRUFEUTtFQUFBLENBQVYsQ0FGQSxDQUFBOztBQUFBLEVBUUEsRUFBRSxDQUFDLFVBQUgsQ0FBYyxhQUFkLEVBQTZCLFNBQUMsTUFBRCxFQUFTLE1BQVQsR0FBQSxDQUE3QixDQVJBLENBQUE7QUFBQSIsInNvdXJjZXNDb250ZW50IjpbIm5nID0gYW5ndWxhci5tb2R1bGUgJ215QXBwJ1xuXG5uZy5jb25maWcgKCRzdGF0ZVByb3ZpZGVyLCBuYXZiYXJQcm92aWRlcikgLT5cbiAgJHN0YXRlUHJvdmlkZXIuc3RhdGUgJ2hvbWUnLFxuICAgIHVybDogJy8nXG4gICAgdGVtcGxhdGVVcmw6ICdqZWVib290Lmh0bWwnXG4gICAgY29udHJvbGxlcjogJ0plZUJvb3RDdHJsJ1xuXG5uZy5jb250cm9sbGVyICdKZWVCb290Q3RybCcsICgkc2NvcGUsIGplZWJ1cykgLT5cbiAgIyBUT0RPIHJld3JpdGUgdGhlc2UgZXhhbXBsZSB0byB1c2UgdGhlIFwiaG1cIiBzZXJ2aWNlIGkucy5vLiBcImplZWJ1c1wiXG4iXX0=
