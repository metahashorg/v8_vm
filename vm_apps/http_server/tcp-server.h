// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_HTTP_SERVER_TCP_SERVER_H_
#define V8_VM_APPS_HTTP_SERVER_TCP_SERVER_H_

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <set>
#include <thread>

#include "src/base/macros.h"
#include "vm_apps/http_server/tcp-server-session.h"
#include "vm_apps/http_server/tcp-server-socket.h"

class TcpServer {
 public:
  TcpServer() ;
  virtual ~TcpServer() ;

  vv::Error Start(
      std::uint16_t port, TcpServerSession::Creator session_creator) ;

  vv::Error Stop() ;

  vv::Error Wait() ;

 private:
  typedef std::set<TcpServerSession*> TcpSessionArray ;

  void Run() ;

  // Callbacks for events from a session
  void OnSessionClosed(TcpServerSession* session) ;
  void OnSessionError(TcpServerSession* session, vv::Error error) ;

  std::unique_ptr<IPEndPoint> ip_endpoint_ ;
  TcpServerSocket socket_ ;

  TcpServerSession::Creator session_creator_ = nullptr ;
  TcpSessionArray sessions_ ;
  std::mutex sessions_lock_ ;
  std::condition_variable sessions_cv_ ;

  std::unique_ptr<std::thread> thread_ ;
  std::atomic<bool> stop_flag_ ;

  DISALLOW_COPY_AND_ASSIGN(TcpServer) ;
};

#endif  // V8_VM_APPS_HTTP_SERVER_TCP_SERVER_H_
