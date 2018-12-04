// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_HTTP_SERVER_TCP_SERVER_SESSION_H_
#define V8_VM_APPS_HTTP_SERVER_TCP_SERVER_SESSION_H_

#include <atomic>
#include <thread>

#include "src/base/macros.h"
#include "vm_apps/http_server/stream-socket.h"
#include "vm_apps/utils/app-utils.h"

class TcpServerSession {
 public:
  class Owner {
   public:
    virtual void OnSessionClose(TcpServerSession* session) = 0 ;
    virtual void OnSessionError(
      TcpServerSession* session, vv::Error error) = 0 ;
  };

  typedef TcpServerSession* (*Creator)(
      std::unique_ptr<StreamSocket>& socket, Owner* owner) ;

  virtual ~TcpServerSession() ;

  virtual vv::Error Start() ;

  virtual vv::Error Stop() ;

  virtual vv::Error Wait() ;

 protected:
  TcpServerSession(std::unique_ptr<StreamSocket>& socket, Owner* owner) ;

  virtual vv::Error Do() = 0 ;

  std::atomic<bool> stop_flag_ ;
  std::unique_ptr<StreamSocket> socket_ ;

 private:
  void Run() ;

  std::unique_ptr<std::thread> thread_ ;
  Owner* owner_ ;

  DISALLOW_COPY_AND_ASSIGN(TcpServerSession) ;
};

#endif  // V8_VM_APPS_HTTP_SERVER_TCP_SERVER_SESSION_H_
