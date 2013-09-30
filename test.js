var http_sync = require('./http-sync')


// Test GET request
var req = http_sync.request({
    host: 'nodejs.org',
    path: '/'
});

// console.log(req);

var res = req.end();
console.log(res);
console.log(res.body.toString());


// Test POST request
req = http_sync.request({
    protocol: 'https',
    method: 'POST',
    host: 'talk.to',
    path: '/bosh/http-bind/',
    body: '<body/>'
});

res = req.end();
console.log(res);
console.log(res.body.toString());

