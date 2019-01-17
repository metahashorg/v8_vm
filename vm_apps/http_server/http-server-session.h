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
   // Handler of a session
   typedef std::function<
       Error(HttpRequestInfo& request, HttpResponseInfo& response)>
       SessionHandler ;

  // Default buffer size for reading/writing a body
  static const std::int32_t kDefaultBodyBufferSize = 16 * 1024 ;

  ~HttpServerSession() override ;

  const HttpRequestInfo* request() const { return request_.get() ; }

  const HttpResponseInfo* response() const { return response_.get() ; }

  // Returns a creator of a http-session
  static Creator GetCreator(
      const SessionHandler& session_handler,
      const std::string& server_name = "",
      std::int32_t body_buffer_size = kDefaultBodyBufferSize) ;

 private:
  HttpServerSession(
      std::unique_ptr<StreamSocket>& socket,
      const SessionHandler& session_handler,
      const std::string& server_name, std::int32_t body_buffer_size) ;

  // Reads a body of a request
  Error GetBody(const char*& body, std::int32_t& body_size, bool& owned) ;

  // Reads a request header and creates a request and a response structures
  Error ReadRequestHeader() ;

  // Sends a response
  Error SendResponse() ;

  // Sets default headers of a resopnse
  void SetResponseDefaultHeaders() ;

  // Entry point of a http-session
  Error Do() override ;

  // Creates a new http-session
  static TcpServerSession* New(
      std::unique_ptr<StreamSocket>& socket,
      const SessionHandler& session_handler,
      const std::string& server_name, std::int32_t body_buffer_size) ;

  // Http-session doesn't have a default constructor
  HttpServerSession() = delete ;

  std::string server_name_ ;
  std::int32_t body_buffer_size_ ;
  std::vector<char> raw_headers_ ;
  std::vector<char> raw_body_ ;

  std::unique_ptr<HttpRequestInfo> request_ ;
  std::unique_ptr<HttpResponseInfo> response_ ;

  SessionHandler session_handler_ ;

  DISALLOW_COPY_AND_ASSIGN(HttpServerSession) ;
};

#endif  // V8_VM_APPS_HTTP_SERVER_HTTP_SERVER_SESSION_H_
