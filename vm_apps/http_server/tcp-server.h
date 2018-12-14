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

  // Starts a tcp-server
  vv::Error Start(
      std::uint16_t port, const TcpServerSession::Creator& session_creator) ;

  // Stops a tcp-server
  vv::Error Stop() ;

  // Waits until tcp-server will have stopped
  vv::Error Wait() ;

  const IPEndPoint* ip_endpoint() const { return ip_endpoint_.get() ; }

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
  TcpServerSession::ClosedCallback session_closed_callback_ ;
  TcpServerSession::ErrorCallback session_error_callback_ ;

  std::unique_ptr<std::thread> thread_ ;
  std::atomic<bool> stop_flag_ ;

  DISALLOW_COPY_AND_ASSIGN(TcpServer) ;
};

#endif  // V8_VM_APPS_HTTP_SERVER_TCP_SERVER_H_
