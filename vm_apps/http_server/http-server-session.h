// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_HTTP_SERVER_HTTP_SERVER_SESSION_H_
#define V8_VM_APPS_HTTP_SERVER_HTTP_SERVER_SESSION_H_

#include "src/base/macros.h"
#include "vm_apps/http_server/http-request-info.h"
#include "vm_apps/http_server/http-response-info.h"
#include "vm_apps/http_server/tcp-server-session.h"


class HttpServerSession : public TcpServerSession {
 public:
  // Default buffer size for reading/writing a body
  static const std::int32_t kDefaultBodyBufferSize = 16 * 1024 ;

  ~HttpServerSession() override ;

  const HttpRequestInfo* request() const { return request_.get() ; }

  const HttpResponseInfo* response() const { return response_.get() ; }

  static Creator GetCreator(
    const std::string& server_name = "",
    std::int32_t body_buffer_size = kDefaultBodyBufferSize) ;

 private:
  HttpServerSession(
      std::unique_ptr<StreamSocket>& socket, const std::string& server_name,
      std::int32_t body_buffer_size) ;

  vv::Error Do() override ;

  static TcpServerSession* New(
      std::unique_ptr<StreamSocket>& socket, const std::string& server_name,
      std::int32_t body_buffer_size) ;

  HttpServerSession() = delete ;

  std::string server_name_ ;
  std::vector<char> raw_headers_ ;
  std::vector<char> raw_body_ ;

  std::unique_ptr<HttpRequestInfo> request_ ;
  std::unique_ptr<HttpResponseInfo> response_ ;

  DISALLOW_COPY_AND_ASSIGN(HttpServerSession) ;
};

#endif  // V8_VM_APPS_HTTP_SERVER_HTTP_SERVER_SESSION_H_
