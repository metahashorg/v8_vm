// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_HTTP_SERVER_V8_HTTP_SERVER_SESSION_H_
#define V8_VM_APPS_HTTP_SERVER_V8_HTTP_SERVER_SESSION_H_

#include "src/base/macros.h"
#include "vm_apps/http_server/http-request-info.h"
#include "vm_apps/http_server/http-response-info.h"

class V8HttpServerSession {
 public:
  struct Request {
    std::uint64_t id = 0 ;
    std::string method ;
    std::string address ;
    struct {
      std::uint64_t value = 0 ;
      std::uint64_t fees = 0 ;
      std::uint64_t nonce = 0 ;
      std::string data_str ;
      struct {
        std::string method ;
        std::string function ;
        std::string code ;
      } data ;
    } transaction ;
  };

  // Entry point of V8 http-session
  static vv::Error ProcessSession(
      HttpRequestInfo& request, HttpResponseInfo& response) ;

 private:
  V8HttpServerSession(HttpRequestInfo& request, HttpResponseInfo& response) ;

  vv::Error Do() ;

  HttpRequestInfo& http_request_ ;
  HttpResponseInfo& http_response_ ;

  std::unique_ptr<Request> request_ ;

  DISALLOW_COPY_AND_ASSIGN(V8HttpServerSession) ;
};

#endif  // V8_VM_APPS_HTTP_SERVER_V8_HTTP_SERVER_SESSION_H_
