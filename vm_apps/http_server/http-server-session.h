// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_HTTP_SERVER_HTTP_SERVER_SESSION_H_
#define V8_VM_APPS_HTTP_SERVER_HTTP_SERVER_SESSION_H_

#include "src/base/macros.h"
#include "vm_apps/http_server/tcp-server-session.h"

class HttpServerSession : public TcpServerSession {
 public:
  ~HttpServerSession() override ;

  static TcpServerSession* New(
      std::unique_ptr<StreamSocket>& socket, Owner* owner) ;

 private:
  HttpServerSession(std::unique_ptr<StreamSocket>& socket, Owner* owner) ;

  vv::Error Do() override ;

  HttpServerSession() = delete ;
  DISALLOW_COPY_AND_ASSIGN(HttpServerSession) ;
};

#endif  // V8_VM_APPS_HTTP_SERVER_HTTP_SERVER_SESSION_H_
