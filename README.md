You will need:

* node.js source code
* v8 source code
* libcurl development package

Building:

If you are using node >= v0.8.0

    node-gyp configure && node-gyp build

If you are using node < v0.8.0

    node-waf configure && node-waf build

Copy Generated library to current directory. Depending on the version of node.js you are using, one of the 2 commands below is the correct one.

    cp build/default/curllib.node .

OR

    cp build/Release/curllib.node .

Run the test.js file:

    node test.js

Using:

```javascript
// Test GET request
var req = http_sync.request({
    host: 'nodejs.org', 
    path: '/'
});

var res = req.end();
console.log(res);
console.log(res.body.toString());
```
