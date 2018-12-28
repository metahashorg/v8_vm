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
  enum class Method {
    Unknown = 0,
    Compile = 1,
    CmdRun = 2,
  };

  struct Request {
    std::uint64_t id = 0 ;
    Method method = Method::Unknown ;
    std::string address_str ;
    std::vector<std::uint8_t> address ;
    std::vector<std::uint8_t> state ;
    struct {
      std::uint64_t value = 0 ;
      std::uint64_t fees = 0 ;
      std::uint64_t nonce = 0 ;
      struct {
        std::string method ;
        std::string function ;
        std::string params ;
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

  // Compiles script
  vv::Error CompileScript() ;

  // Runs a command script
  vv::Error RunCommandScript() ;

  vv::Error WriteResponseBody() ;

  HttpRequestInfo& http_request_ ;
  HttpResponseInfo& http_response_ ;

  std::unique_ptr<Request> request_ ;
  std::string response_state_ ;
  std::string response_address_ ;

  DISALLOW_COPY_AND_ASSIGN(V8HttpServerSession) ;
};

#endif  // V8_VM_APPS_HTTP_SERVER_V8_HTTP_SERVER_SESSION_H_
