/* -*- indent-tabs-mode: nil; c-basic-offset: 2; tab-width: 2 -*- */

/* This code is PUBLIC DOMAIN, and is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND. See the accompanying
 * LICENSE file.
 */

#include <nan.h>
#include <curl/curl.h>
#include <string>
#include <string.h>
#include <vector>
#include <stdint.h>
#include <iostream>

using namespace node;
using namespace v8;

#define THROW_BAD_ARGS \
  Nan::ThrowTypeError("Bad argument")

#define LOCAL_STRING_HANDLE(text) \
  Nan::New<String>(text).ToLocalChecked()

#define PERSISTENT_STRING(id, text) \
  id.Reset(LOCAL_STRING_HANDLE(text))

#define IS_STRING_PROP(obj, key) \
  Nan::Get(obj, key).ToLocalChecked()->IsString()

#define IS_NUMBER_PROP(obj, key) \
  Nan::Get(obj, key).ToLocalChecked()->IsNumber()

#define HAS_STRING_PROP(obj, key) \
  Nan::Has(obj, key).FromJust() && IS_STRING_PROP(obj, key)

#define HAS_NUMBER_PROP(obj, key) \
  Nan::Has(obj, key).FromJust() && IS_NUMBER_PROP(obj, key)

#define GET_STRING_PROP(obj, key) \
  Nan::Get(obj, key).ToLocalChecked()->ToString()

#define GET_INT_PROP(obj, key) \
  Nan::Get(obj, key).ToLocalChecked()->IntegerValue()

typedef std::vector<char> buff_t;

class CurlLib : Nan::ObjectWrap {
private:
  static std::string buffer;
  static std::vector<std::string> headers;
  static Nan::Persistent<String> sym_body_length;
  static Nan::Persistent<String> sym_headers;
  static Nan::Persistent<String> sym_timedout;
  static Nan::Persistent<String> sym_error;

public:
  static Nan::Persistent<Function> s_constructor;
  static void Init(Handle<Object> target) {
    Local<FunctionTemplate> t = Nan::New<FunctionTemplate>(New);
    Local<String> className = LOCAL_STRING_HANDLE("CurlLib");

    t->InstanceTemplate()->SetInternalFieldCount(1);
    t->SetClassName(className);

    Nan::SetPrototypeMethod(t, "run", Run);
    Nan::SetPrototypeMethod(t, "body", Body);

    Local<Function> tFunction = Nan::GetFunction(t).ToLocalChecked();
    s_constructor.Reset(tFunction);
    Nan::Set(target, className, tFunction);

    PERSISTENT_STRING(sym_body_length, "body_length");
    PERSISTENT_STRING(sym_headers, "headers");
    PERSISTENT_STRING(sym_timedout, "timedout");
    PERSISTENT_STRING(sym_error, "error");
  }

  CurlLib() { }
  ~CurlLib() { }

  static NAN_METHOD(New) {
    CurlLib* curllib = new CurlLib();
    curllib->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  static size_t write_data(void *ptr, size_t size,
			   size_t nmemb, void *userdata) {
    buffer.append(static_cast<char*>(ptr), size * nmemb);
    // std::cerr<<"Wrote: "<<size*nmemb<<" bytes"<<std::endl;
    // std::cerr<<"Buffer size: "<<buffer.size()<<" bytes"<<std::endl;
    return size * nmemb;
  }

  static size_t write_headers(void *ptr, size_t size, size_t nmemb, void *userdata)
  {
    std::string header(static_cast<char*>(ptr), size * nmemb);
    headers.push_back(header);
    return size * nmemb;
  }

  static NAN_METHOD(Body) {

    if (info.Length() < 1 || !Buffer::HasInstance(info[0])) {
      return THROW_BAD_ARGS;
    }

    Local<Object> buffer_obj = info[0]->ToObject();
    char *buffer_data        = Buffer::Data(buffer_obj);
    size_t buffer_length     = Buffer::Length(buffer_obj);

    if (buffer_length < buffer.size()) {
      return Nan::ThrowTypeError("Insufficient Buffer Length");
    }

    if (!buffer.empty()) {
      memcpy(buffer_data, buffer.data(), buffer.size());
    }
    buffer.clear();
    info.GetReturnValue().Set(buffer_obj);
  }

  static NAN_METHOD(Run) {

    if (info.Length() < 1) {
      return THROW_BAD_ARGS;
    }

    Local<String> key_method = LOCAL_STRING_HANDLE("method");
    Local<String> key_url = LOCAL_STRING_HANDLE("url");
    Local<String> key_headers = LOCAL_STRING_HANDLE("headers");
    Local<String> key_body = LOCAL_STRING_HANDLE("body");
    Local<String> key_connect_timeout_ms = LOCAL_STRING_HANDLE("connect_timeout_ms");
    Local<String> key_timeout_ms = LOCAL_STRING_HANDLE("timeout_ms");
    Local<String> key_rejectUnauthorized = LOCAL_STRING_HANDLE("rejectUnauthorized");
    Local<String> key_caCert = LOCAL_STRING_HANDLE("ca");
    Local<String> key_clientCert = LOCAL_STRING_HANDLE("cert");
    Local<String> key_pfx = LOCAL_STRING_HANDLE("pfx");
    Local<String> key_clientKey = LOCAL_STRING_HANDLE("key");
    Local<String> key_clientKeyPhrase = LOCAL_STRING_HANDLE("passphrase");

    static const Local<String> PFXFORMAT = LOCAL_STRING_HANDLE("P12");

    Local<Array> opt = Local<Array>::Cast(info[0]);

    if (!Nan::Has(opt, key_method).FromJust() ||
        !Nan::Has(opt, key_url).FromJust() ||
        !Nan::Has(opt, key_headers).FromJust()) {
      return THROW_BAD_ARGS;
    }

    if (!IS_STRING_PROP(opt, key_method) ||
        !IS_STRING_PROP(opt, key_url)) {
      return THROW_BAD_ARGS;
    }

    Local<String> method = GET_STRING_PROP(opt, key_method);
    Local<String> url    = GET_STRING_PROP(opt, key_url);
    Local<Array>  reqh   = Local<Array>::Cast(Nan::Get(opt, key_headers).ToLocalChecked());
    Local<String> body   = Nan::EmptyString();
    Local<String> caCert   = Nan::EmptyString();
    Local<String> clientCert   = Nan::EmptyString();
    Local<String> clientCertFormat   = Nan::EmptyString();
    Local<String> clientKey   = Nan::EmptyString();
    Local<String> clientKeyPhrase   = Nan::EmptyString();
    long connect_timeout_ms = 1 * 60 * 60 * 1000; /* 1 hr in msec */
    long timeout_ms = 1 * 60 * 60 * 1000; /* 1 hr in msec */
    bool rejectUnauthorized = false;

    if (HAS_STRING_PROP(opt, key_caCert)) {
      caCert = GET_STRING_PROP(opt, key_caCert);
    }

    if (HAS_STRING_PROP(opt, key_clientKey)) {
      clientKey = GET_STRING_PROP(opt, key_clientKey);
    }

    if (HAS_STRING_PROP(opt, key_clientKeyPhrase)) {
      clientKeyPhrase = GET_STRING_PROP(opt, key_clientKeyPhrase);
    }

    if (HAS_STRING_PROP(opt, key_clientCert)) {
      clientCert = GET_STRING_PROP(opt, key_clientCert);
    } else if (HAS_STRING_PROP(opt, key_pfx)) {
      clientCert = GET_STRING_PROP(opt, key_pfx);
      clientCertFormat = PFXFORMAT;
    }

    if (HAS_STRING_PROP(opt, key_body)) {
      body = GET_STRING_PROP(opt, key_body);
    }

    if (HAS_NUMBER_PROP(opt, key_connect_timeout_ms)) {
      connect_timeout_ms = GET_INT_PROP(opt, key_connect_timeout_ms);
    }

    if (HAS_NUMBER_PROP(opt, key_timeout_ms)) {
      timeout_ms = GET_INT_PROP(opt, key_timeout_ms);
    }

    if (Nan::Has(opt, key_rejectUnauthorized).FromJust()) {
      Local<Value> rejectUnauthorizedValue =
        Nan::Get(opt, key_rejectUnauthorized).ToLocalChecked();

      // std::cerr<<"has reject unauth"<<std::endl;
      if (rejectUnauthorizedValue->IsBoolean()) {
        rejectUnauthorized = rejectUnauthorizedValue->BooleanValue();
      } else if (rejectUnauthorizedValue->IsBooleanObject()) {
        rejectUnauthorized = rejectUnauthorizedValue
          ->ToBoolean()
          ->BooleanValue();
      }
    }

    // std::cerr<<"rejectUnauthorized: " << rejectUnauthorized << std::endl;

    Nan::Utf8String _body(body);
    Nan::Utf8String _method(method);
    Nan::Utf8String _url(url);
    Nan::Utf8String _cacert(caCert);
    Nan::Utf8String _clientcert(clientCert);
    Nan::Utf8String _clientcertformat(clientCertFormat);
    Nan::Utf8String _clientkeyphrase(clientKeyPhrase);
    Nan::Utf8String _clientkey(clientKey);

    std::vector<std::string> _reqh;
    for (size_t i = 0; i < reqh->Length(); ++i) {
      _reqh.push_back(*Nan::Utf8String(Nan::Get(reqh, i).ToLocalChecked()));
    }

    // CurlLib* curllib = Nan::ObjectWrap::Unwrap<CurlLib>(info.This());

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

      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, *_method);
      if (_body.length() > 0) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, *_body);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                         (curl_off_t)_body.length());
      }
      curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
      curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5);
      curl_easy_setopt(curl, CURLOPT_URL, *_url);
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

      if (_cacert.length() > 0) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, *_cacert);
      }

      if (_clientcert.length() > 0) {
        if (_clientcertformat.length() > 0) {
          curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, *_clientcertformat);
        }
        curl_easy_setopt(curl, CURLOPT_SSLCERT, *_clientcert);
      }

      if (_clientkeyphrase.length() > 0) {
        curl_easy_setopt(curl, CURLOPT_KEYPASSWD, *_clientkeyphrase);
      }

      if (_clientkey.length() > 0) {
        curl_easy_setopt(curl, CURLOPT_SSLKEY, *_clientkey);
      }

      struct curl_slist *slist = NULL;

      for (size_t i = 0; i < _reqh.size(); ++i) {
        slist = curl_slist_append(slist, _reqh[i].c_str());
      }

      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);

      res = curl_easy_perform(curl);

      curl_slist_free_all(slist);

      /* always cleanup */
      curl_easy_cleanup(curl);
    }

    // std::cerr<<"error_buffer: "<<error_buffer<<std::endl;

    Local<Object> result = Nan::New<Object>();

    if (!res) {
      Nan::Set(result, Nan::New(sym_body_length), Nan::New<Integer>((int32_t)buffer.size()));
      Local<Array> _h = Nan::New<Array>();
      for (size_t i = 0; i < headers.size(); ++i) {
        Nan::Set(_h, i, Nan::New<String>(headers[i].c_str()).ToLocalChecked());
      }
      Nan::Set(result, Nan::New<String>(sym_headers), _h);
    }
    else if (res == CURLE_OPERATION_TIMEDOUT) {
      Nan::Set(result, Nan::New<String>(sym_timedout), Nan::New<Integer>(1));
    } else {
      Nan::Set(result, Nan::New<String>(sym_error), LOCAL_STRING_HANDLE(curl_easy_strerror(res)));
    }

    headers.clear();
    info.GetReturnValue().Set(result);
  }
};

Nan::Persistent<Function> CurlLib::s_constructor;
std::string CurlLib::buffer;
std::vector<std::string> CurlLib::headers;
Nan::Persistent<String> CurlLib::sym_body_length;
Nan::Persistent<String> CurlLib::sym_headers;
Nan::Persistent<String> CurlLib::sym_timedout;
Nan::Persistent<String> CurlLib::sym_error;

extern "C" {
  static void init (Handle<Object> target) {
    CurlLib::Init(target);
  }
  NODE_MODULE(curllib, init);
}

