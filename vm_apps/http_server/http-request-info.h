// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_HTTP_SERVER_HTTP_REQUEST_INFO_H_
#define V8_VM_APPS_HTTP_SERVER_HTTP_REQUEST_INFO_H_

#include "vm_apps/http_server/http-package-info.h"

class HttpRequestInfo : public HttpPackageInfo {
 public:
  struct Method {
    // Request method names. The method is case-sensitive.
    // https://tools.ietf.org/html/rfc2616#section-5.1.1
    static const char Connect[] ;
    static const char Delete[] ;
    static const char Get[] ;
    static const char Head[] ;
    static const char Options[] ;
    static const char Post[] ;
    static const char Put[] ;
    static const char Trace[] ;
  };

  struct Header {
    // Request specific header names.
    // https://tools.ietf.org/html/rfc2616#section-5.3
    // Field names are case-insensitive.
    // https://tools.ietf.org/html/rfc2616#section-4.2
    static const char Accept[] ;
    static const char AcceptCharset[] ;
    static const char AcceptEncoding[] ;
    static const char AcceptLanguage[] ;
    static const char Authorization[] ;
    static const char Cookie[] ;
    static const char Expect[] ;
    static const char From[] ;
    static const char Host[] ;
    static const char IfMatch[] ;
    static const char IfModifiedSince[] ;
    static const char IfNoneMatch[] ;
    static const char IfRange[] ;
    static const char IfUnmodifiedSince[] ;
    static const char MaxForwards[] ;
    static const char ProxyAuthorization[] ;
    static const char Range[] ;
    static const char Referer[] ;
    static const char TE[] ;
    static const char UserAgent[] ;
  };

  HttpRequestInfo() ;
  ~HttpRequestInfo() override ;

  // Clears the object
  void Clear() override ;

  // Serializes HttpPackageInfo to a string representation.  Joins all the
  // header keys and values with ": ", and inserts "\r\n" between each header
  // line, and adds the trailing "\r\n". Body won't be included.
  std::string ToString() const override ;

  const std::string& method() const { return method_ ; }

  const std::string& uri() const { return uri_ ; }

  const std::string& host() const { return host_ ; }

  vv::Error raw_request(const char*& request, std::int32_t& size) const {
    request = raw_request_ ;
    size = raw_request_size_ ;
    return raw_request_error_ ;
  }

 protected:
   // Initializes the object by raw data
  vv::Error ParseInternal(
      const char* request, std::int32_t size, bool owned = false) override ;

  void UpdateInfoByHeader(
       const std::string& key, const std::string& value,
       bool deleted = false) override ;

 private:
  std::string method_ ;
  std::string uri_ ;
  std::string host_ ;

  // Request data
  const char* raw_request_ = nullptr ;
  std::int32_t raw_request_size_ = 0 ;
  bool raw_request_owned_ = false ;
  vv::Error raw_request_error_ = vv::errObjNotInit ;

  DISALLOW_COPY_AND_ASSIGN(HttpRequestInfo) ;
};

#endif  // V8_VM_APPS_HTTP_SERVER_HTTP_REQUEST_INFO_H_
