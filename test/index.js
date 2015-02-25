'use strict';
var spawn = require('child_process').spawn;
var path = require('path');
var assert = require('assert');

var request = require('../').request;

var webPort = 4490;
var echoServerPath = path.join(__dirname, 'echo-server');

function mkReq(path, options) {
  options = options || {};
  options.host = '127.0.0.1';
  options.port = webPort;
  options.path = path;
  var res = request(options).end();
  res.echo = JSON.parse(res.body.toString());
  return res;
}

var tests = [
  function simpleGET() {
    var res = mkReq('/x');
    assert.equal(res.echo.method, 'GET');
    assert.equal(res.echo.url, '/x');
    assert.equal(res.echo.headers.accept, '*/*');
  },
  function simplePOST() {
    var body = '{"x":42}';
    var res = mkReq('/', { method: 'POST', body: body });
    assert.equal(res.echo.method, 'POST');
    assert.equal(res.echo.body, body);
  },
  function sendHeaders() {
    var headers = {
      'X-Foo': 'bar'
    };
    var res = mkReq('/', { headers: headers });
    assert.equal(res.echo.headers['x-foo'], headers['X-Foo']);
  },
  function postJSON() {
    var body = '{"x":true}';
    var headers = {
      'Content-Type': 'application/json'
    };
    var res = mkReq('/', { headers: headers, body: body });
    assert.equal(res.echo.headers['content-type'], headers['Content-Type']);
    assert.equal(res.echo.body, body);
  },
  function alwaysPass() {}
];

function runTests() { // ad-hoc reinvention of tape
  console.log('1..%d', tests.length);
  // simple GET request
  var failed = tests.filter(function(t, idx) {
    try {
      t();
      console.log('ok %d - %s', idx + 1, t.name);
      return false;
    } catch (err) {
      console.log('not ok %d - %s', idx + 1, t.name);
      console.log('  ' + err.stack.split('\n').join('\n  '));
      return true;
    }
  });

  process.exit(failed.length ? 1 : 0);
}

var echoServer = spawn(echoServerPath, [webPort]);
process.on('exit', function() {
  if (echoServer) {
    try {
      echoServer.kill();
    }
    catch (err) {
      console.error(err.stack);
    }
  }
});
setTimeout(runTests, 200);
