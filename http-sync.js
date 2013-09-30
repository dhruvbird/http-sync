var _cl = require('./curllib');
var curllib = new _cl.CurlLib();

function CurlRequest(options) {
    this._options = options;
    this._headers = { };

    var k;
    for (k in this._options.headers) {
        this.setHeader(k, this._options.headers[k]);
    }
}

CurlRequest.prototype = {
    getHeader: function(name) {
        return this._headers[name.toLowerCase()];
    },
    removeHeader: function(name) {
        delete this._headers[name.toLowerCase()];
    },
    setHeader: function(name, value) {
        this._headers[name.toLowerCase()] = value;
    },
    write: function(data) {
        data = data || '';
        this._options.body += data;
    },
    end: function(data) {
        this.write(data);
        var _ep = this._options.protocol + '://' + this._options.host +
            ':' + this._options.port + this._options.path;
        var _h = [ ];
        var k;
        for (k in this._headers) {
            _h.push(k + ': ' + this._headers[k]);
        }

        var ret = curllib.run(this._options.method, _ep,
                      _h, this._options.body);

        if (ret.error) {
            throw new Error(ret.error);
        }

        ret.body = '';
        if (ret.body_length) {
            var _b = new Buffer(ret.body_length);
            ret.body = curllib.body(_b);
        }

        function _parse_headers(headers) {
            var _sre = /HTTP\/[0-9].[0-9] ([0-9]{3})/;
            var _hre = /([^:]+):([\s]*)([^\r\n]*)/;
            var statusCode = 200;
            var _h = { };
            headers.forEach(function(line) {
                var _m = line.match(_sre);
                if (_m) {
                    statusCode = _m[1];
                }
                else {
                    _m = line.match(_hre);
                    if (_m) {
                        _h[_m[1]] = _m[3];
                    }
                }
            });
            return {
                statusCode: statusCode,
                headers: _h
            };
        }

        var _ph = _parse_headers(ret.headers);
        ret.statusCode = _ph.statusCode;
        ret.headers    = _ph.headers;

        return ret;
    }
};

exports.request = function(options) {
    options.protocol = options.protocol || 'http';
    options.port = options.port || (options.protocol === 'https' ? 443 : 80);
    options.method = options.method || 'GET';
    options.path = options.path || '/';
    options.headers = options.headers || { };
    options.host = options.host || '127.0.0.1';
    options.body = options.body || '';

    return new CurlRequest(options);
};
