// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/http-server-session.h"

HttpServerSession::~HttpServerSession() {}

TcpServerSession::Creator HttpServerSession::GetCreator(
    const std::string& server_name, std::int32_t body_buffer_size) {
  return std::bind(
      &HttpServerSession::New, std::placeholders::_1, server_name,
      body_buffer_size) ;
}

HttpServerSession::HttpServerSession(
    std::unique_ptr<StreamSocket>& socket, const std::string& server_name,
    std::int32_t body_buffer_size)
  : TcpServerSession(socket),
    server_name_(server_name) {}

vv::Error HttpServerSession::Do() {
  printf("VERBS: HttpServerSession::Do()\n") ;

  return vv::errOk ;
}

TcpServerSession* HttpServerSession::New(
    std::unique_ptr<StreamSocket>& socket, const std::string& server_name,
    std::int32_t body_buffer_size) {
   return new HttpServerSession(socket, server_name, body_buffer_size) ;
}
