/* -*- indent-tabs-mode: nil; c-basic-offset: 2; tab-width: 2 -*- */

/* This code is PUBLIC DOMAIN, and is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND. See the accompanying
 * LICENSE file.
 */

#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <curl/curl.h>
#include <string>
#include <string.h>
#include <vector>
#include <stdint.h>
#include <iostream>

using namespace node;
using namespace v8;

#define THROW_BAD_ARGS \
  ThrowException(Exception::TypeError(String::New("Bad argument")))

typedef std::vector<char> buff_t;

class CurlLib : ObjectWrap {
private:
  static std::string buffer;
  static std::vector<std::string> headers;
  static Persistent<String> sym_body_length;
  static Persistent<String> sym_headers;
  static Persistent<String> sym_timedout;
  static Persistent<String> sym_error;

public:
  static Persistent<FunctionTemplate> s_ct;
  static void Init(Handle<Object> target) {
    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("CurlLib"));

    NODE_SET_PROTOTYPE_METHOD(s_ct, "run", Run);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "body", Body);

    target->Set(String::NewSymbol("CurlLib"),
                s_ct->GetFunction());

    sym_body_length = NODE_PSYMBOL("body_length");
    sym_headers = NODE_PSYMBOL("headers");
    sym_timedout = NODE_PSYMBOL("timedout");
    sym_error = NODE_PSYMBOL("error");
  }

  CurlLib() { }
  ~CurlLib() { }

  static Handle<Value> New(const Arguments& args) {
    CurlLib* curllib = new CurlLib();
    curllib->Wrap(args.This());
    return args.This();
  }

  static size_t write_data(void *ptr, size_t size,
			   size_t nmemb, void *userdata) {
    buffer += std::string((char*)ptr, size*nmemb);
    //std::cerr<<"Wrote: "<<size*nmemb<<" bytes"<<std::endl;
    //std::cerr<<"Buffer size: "<<buffer.size()<<" bytes"<<std::endl;
    return size * nmemb;
  }

  static size_t write_headers(void *ptr, size_t size, size_t nmemb, void *userdata)
  {
    std::string header((char*)ptr, size*nmemb);
    headers.push_back(header);
    return size * nmemb;
  }

  static void copy_to_buffer(buff_t &dest, Local<String> &src) {
    //std::cerr<<"copy_to_buffer::Length::"<<src->Length()<<std::endl;

    if (src->Length() > 0) {
      dest.resize(src->Length() + 1);
      src->WriteAscii(&dest[0], 0, src->Length());
    }
  }

  static Handle<Value> Body(const Arguments& args) {
    if (args.Length() < 1 || !Buffer::HasInstance(args[0])) {
      return THROW_BAD_ARGS;
    }

    HandleScope scope;
    Local<Object> buffer_obj = args[0]->ToObject();
    char *buffer_data        = Buffer::Data(buffer_obj);
    size_t buffer_length     = Buffer::Length(buffer_obj);

    if (buffer_length < buffer.size()) {
      return ThrowException(Exception::TypeError(
        String::New("Insufficient Buffer Length")));
    }

    if (!buffer.empty()) {
      memcpy(buffer_data, &buffer[0], buffer.size());
    }
    buffer.clear();
    return scope.Close(buffer_obj);
  }

  static Handle<Value> Run(const Arguments& args) {
    if (args.Length() < 1) {
      return THROW_BAD_ARGS;
    }

    Local<String> key_method = String::New("method");
    Local<String> key_url = String::New("url");
    Local<String> key_headers = String::New("headers");
    Local<String> key_body = String::New("body");
    Local<String> key_connect_timeout_ms = String::New("connect_timeout_ms");
    Local<String> key_timeout_ms = String::New("timeout_ms");
    Local<String> key_rejectUnauthorized = String::New("rejectUnauthorized");

    Local<Array> opt = Local<Array>::Cast(args[0]);

    if (!opt->Has(key_method) ||
        !opt->Has(key_url) ||
        !opt->Has(key_headers)) {
      return THROW_BAD_ARGS;
    }

    if (!opt->Get(key_method)->IsString() ||
        !opt->Get(key_url)->IsString()) {
      return THROW_BAD_ARGS;
    }

    Local<String> method = Local<String>::Cast(opt->Get(key_method));
    Local<String> url    = Local<String>::Cast(opt->Get(key_url));
    Local<Array>  reqh   = Local<Array>::Cast(opt->Get(key_headers));
    Local<String> body   = String::New((const char*)"", 0);
    long connect_timeout_ms = 1 * 60 * 60 * 1000; /* 1 hr in msec */
    long timeout_ms = 1 * 60 * 60 * 1000; /* 1 hr in msec */
    bool rejectUnauthorized = false;

    if (opt->Has(key_body) && opt->Get(key_body)->IsString()) {
      body = opt->Get(key_body)->ToString();
    }

    if (opt->Has(key_connect_timeout_ms) && opt->Get(key_connect_timeout_ms)->IsNumber()) {
      connect_timeout_ms = opt->Get(key_connect_timeout_ms)->IntegerValue();
    }

    if (opt->Has(key_timeout_ms) && opt->Get(key_timeout_ms)->IsNumber()) {
      timeout_ms = opt->Get(key_timeout_ms)->IntegerValue();
    }

    if (opt->Has(key_rejectUnauthorized)) {
      // std::cerr<<"has reject unauth"<<std::endl;
      if (opt->Get(key_rejectUnauthorized)->IsBoolean()) {
        rejectUnauthorized = opt->Get(key_rejectUnauthorized)->BooleanValue();
      } else if (opt->Get(key_rejectUnauthorized)->IsBooleanObject()) {
        rejectUnauthorized = opt->Get(key_rejectUnauthorized)
          ->ToBoolean()
          ->BooleanValue();
      }
    }

    // std::cerr<<"rejectUnauthorized: " << rejectUnauthorized << std::endl;

    buff_t _body, _method, _url;
    std::vector<buff_t> _reqh;

    copy_to_buffer(_body, body);
    if (!_body.empty()) {
      _body.resize(_body.size() - 1);
    }

    copy_to_buffer(_method, method);
    copy_to_buffer(_url, url);

    for (size_t i = 0; i < reqh->Length(); ++i) {
      buff_t _tmp;
      Local<String> _src = reqh->Get(i)->ToString();
      copy_to_buffer(_tmp, _src);
      _reqh.push_back(_tmp);
    }

    HandleScope scope;
    // CurlLib* curllib = ObjectWrap::Unwrap<CurlLib>(args.This());

    buffer.clear();
    headers.clear();

    CURL *curl;
    CURLcode res = CURLE_FAILED_INIT;

    // char error_buffer[CURL_ERROR_SIZE];
    // error_buffer[0] = '\0';

    curl = curl_easy_init();
    if (curl) {
      // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
      // curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);

      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, &_method[0]);
      if (_body.size()) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS,
			 reinterpret_cast<char*> (&_body[0]));
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE,
			 (curl_off_t)-1);
      }
      curl_easy_setopt(curl, CURLOPT_URL, &_url[0]);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
      curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_headers);

      curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, connect_timeout_ms);
      curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);

      if (rejectUnauthorized) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
      } else {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
      }

      struct curl_slist *slist = NULL;

      for (size_t i = 0; i < _reqh.size(); ++i) {
        slist = curl_slist_append(slist, &(_reqh[i][0]));
      }

      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);

      res = curl_easy_perform(curl);

      curl_slist_free_all(slist);

      /* always cleanup */
      curl_easy_cleanup(curl);
    }

    // std::cerr<<"error_buffer: "<<error_buffer<<std::endl;

    Local<Object> result = Object::New();

    if (!res) {
      result->Set(sym_body_length, Integer::New(buffer.size()));
      Local<Array> _h = Array::New();
      for (size_t i = 0; i < headers.size(); ++i) {
        _h->Set(i, String::New(headers[i].c_str()));
      }
      result->Set(sym_headers, _h);
    }
    else if (res == CURLE_OPERATION_TIMEDOUT) {
      result->Set(sym_timedout, Integer::New(1));
    } else {
      result->Set(sym_error, String::New(curl_easy_strerror(res)));
    }

    // buffer.clear();
    headers.clear();

    return scope.Close(result);
  }
};

Persistent<FunctionTemplate> CurlLib::s_ct;
std::string CurlLib::buffer;
std::vector<std::string> CurlLib::headers;
Persistent<String> CurlLib::sym_body_length;
Persistent<String> CurlLib::sym_headers;
Persistent<String> CurlLib::sym_timedout;
Persistent<String> CurlLib::sym_error;

extern "C" {
  static void init (Handle<Object> target) {
    CurlLib::Init(target);
  }
  NODE_MODULE(curllib, init);
}
