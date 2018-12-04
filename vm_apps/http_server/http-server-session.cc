// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/http-server-session.h"

HttpServerSession::~HttpServerSession() {}

TcpServerSession* HttpServerSession::New(
    std::unique_ptr<StreamSocket>& socket, Owner* owner) {
  return new HttpServerSession(socket, owner) ;
}

HttpServerSession::HttpServerSession(
    std::unique_ptr<StreamSocket>& socket, Owner* owner)
  : TcpServerSession(socket, owner) {}

vv::Error HttpServerSession::Do() {
  printf("VERBS: HttpServerSession::Do()\n") ;
  return vv::errOk ;
}
