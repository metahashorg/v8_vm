// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_HTTP_SERVER_HTTP_RESPONSE_INFO_H_
#define V8_VM_APPS_HTTP_SERVER_HTTP_RESPONSE_INFO_H_

#include "vm_apps/http_server/http-package-info.h"
#include "vm_apps/http_server/http-status-code.h"

class HttpResponseInfo : public HttpPackageInfo {
 public:
  struct Header {
    // Response specific header names.
    // https://tools.ietf.org/html/rfc2616#section-6.2
    // Field names are case-insensitive.
    // https://tools.ietf.org/html/rfc2616#section-4.2
    static const char AcceptRanges[] ;
    static const char Age[] ;
    static const char ETag[] ;
    static const char Location[] ;
    static const char ProxyAuthenticate[] ;
    static const char RetryAfter[] ;
    static const char Server[] ;
    static const char SetCookie[] ;
    static const char Vary[] ;
    static const char WWWAuthenticate[] ;
  };

  HttpResponseInfo() ;
  HttpResponseInfo(std::int32_t status_code) ;
  ~HttpResponseInfo() override ;

  // Clears the object
  void Clear() override ;

  // Sets a status codes
  vv::Error SetStatusCode(std::int32_t status_code) ;

  // Serializes HttpPackageInfo to a string representation.  Joins all the
  // header keys and values with ": ", and inserts "\r\n" between each header
  // line, and adds the trailing "\r\n". Body won't be included.
  std::string ToString() const override ;

  HttpStatusCode status_code() const { return status_code_ ; }

 protected:
  // Initializes the object by raw data
  vv::Error ParseInternal(
      const char* response, std::int32_t size, bool owned = false) override ;

 private:
  HttpStatusCode status_code_ = HTTP_OK ;

  // Response data
  const char* raw_response_ = nullptr ;
  std::int32_t raw_response_size_ = 0 ;
  bool raw_response_owned_ = false ;
  vv::Error raw_response_error_ = vv::errObjNotInit ;

  DISALLOW_COPY_AND_ASSIGN(HttpResponseInfo) ;
};

#endif  // V8_VM_APPS_HTTP_SERVER_HTTP_RESPONSE_INFO_H_
