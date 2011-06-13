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

public:

  static Persistent<FunctionTemplate> s_ct;
  static void Init(Handle<Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("CurlLib"));

    NODE_SET_PROTOTYPE_METHOD(s_ct, "run", Run);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "body", Body);

    target->Set(String::NewSymbol("CurlLib"),
                s_ct->GetFunction());
  }

  CurlLib()
  {
  }

  ~CurlLib()
  {
  }

  static Handle<Value> New(const Arguments& args)
  {
    HandleScope scope;
    CurlLib* curllib = new CurlLib();
    curllib->Wrap(args.This());
    return args.This();
  }

  static size_t read_data(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t len = size * nmemb;
    buff_t *pin = (buff_t*)(userdata);
    
    size_t to_write = (pin->size() > len ? len : pin->size());
    if (to_write == 0) {
      return 0;
    }

    memcpy(ptr, &*(pin->begin()), to_write);
    pin->erase(pin->begin(), pin->begin() + to_write);
    return to_write;
  }

  static size_t write_data(void *ptr, size_t size, size_t nmemb, void *userdata)
  {
    buffer += std::string((char*)ptr, size*nmemb);
    std::cerr<<"Wrote: "<<size*nmemb<<" bytes"<<std::endl;
    std::cerr<<"Buffer size: "<<buffer.size()<<" bytes"<<std::endl;
    return size * nmemb;
  }

  static size_t write_headers(void *ptr, size_t size, size_t nmemb, void *userdata)
  {
    std::string header((char*)ptr, size*nmemb);
    headers.push_back(header);
    return size * nmemb;
  }

  static void copy_to_buffer(buff_t &dest, Local<String> &src) {
    std::cerr<<"copy_to_buffer::Length::"<<src->Length()<<std::endl;

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
    if (args.Length() < 3 || !args[0]->IsString() || !args[1]->IsString() || !args[2]->IsArray()) {
      return THROW_BAD_ARGS;
    }

    if (args.Length() > 3 && !args[3]->IsString()) {
      return THROW_BAD_ARGS;
    }
    
    Local<String> method = args[0]->ToString();
    Local<String> url    = args[1]->ToString();
    Local<Array>  reqh   = Local<Array>::Cast(args[2]);
    Local<String> body   = String::New((const char*)"", 0);

    if (args.Length() > 3) {
      body = args[3]->ToString();
    }

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
    CurlLib* curllib = ObjectWrap::Unwrap<CurlLib>(args.This());
    // Local<String> result = String::New("Hello World");

    buffer.clear();
    headers.clear();

    CURL *curl;
    CURLcode res;

    // char error_buffer[CURL_ERROR_SIZE];
    // error_buffer[0] = '\0';

    curl = curl_easy_init();
    if(curl) {
      // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
      // curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);

      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, &_method[0]);
      curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_data);
      curl_easy_setopt(curl, CURLOPT_READDATA, (void*)&_body);
      curl_easy_setopt(curl, CURLOPT_URL, &_url[0]);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
      curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_headers);

      // FIXME
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

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

    // Local<String> result = String::New("Hello World");
    Local<Object> result = Object::New();

    if (!res) {
      result->Set(NODE_PSYMBOL("body_length"), Integer::New(buffer.size()));
      Local<Array> _h = Array::New();
      for (size_t i = 0; i < headers.size(); ++i) {
	_h->Set(i, String::New(headers[i].c_str()));
      }
      result->Set(NODE_PSYMBOL("headers"), _h);
    }
    else {
      result->Set(NODE_PSYMBOL("error"), String::New(curl_easy_strerror(res)));
    }

    // buffer.clear();
    headers.clear();

    return scope.Close(result);
  }

};

Persistent<FunctionTemplate> CurlLib::s_ct;
std::string CurlLib::buffer;
std::vector<std::string> CurlLib::headers;


extern "C" {
  static void init (Handle<Object> target)
  {
    CurlLib::Init(target);
  }

  NODE_MODULE(curllib, init);
}
