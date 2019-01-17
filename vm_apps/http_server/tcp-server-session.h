// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_HTTP_SERVER_TCP_SERVER_SESSION_H_
#define V8_VM_APPS_HTTP_SERVER_TCP_SERVER_SESSION_H_

#include <atomic>
#include <functional>
#include <thread>

#include "src/base/macros.h"
#include "vm_apps/http_server/stream-socket.h"
#include "vm_apps/utils/app-utils.h"

class TcpServerSession {
 public:
  // Function of a session creator
  typedef std::function<
      TcpServerSession*(std::unique_ptr<StreamSocket>& socket)> Creator ;

  // Callback for session closed
  typedef std::function<void(TcpServerSession* session)> ClosedCallback ;

  // Callback for a session error
  typedef std::function<
      void(TcpServerSession* session, Error error)> ErrorCallback ;

  virtual ~TcpServerSession() ;

  // Starts tcp-session
  virtual Error Start() ;

  // Stops tcp-session
  virtual Error Stop() ;

  // Waits until tcp-session will have stopped
  virtual Error Wait() ;

  // Sets callback for session closed
  void SetClosedCallback(const ClosedCallback& callback) ;

  // Sets callback for a session error
  void SetErrorCallback(const ErrorCallback& callback) ;

 protected:
  TcpServerSession(std::unique_ptr<StreamSocket>& socket) ;

  // Reads data from a socket
  Error Read(char* buf, std::int32_t& buf_len, bool complete_buf = false) ;

  // Writes data from a socket
  Error Write(const char* buf, std::int32_t& buf_len) ;

  virtual Error Do() = 0 ;


 private:
  void Run() ;

  std::atomic<bool> stop_flag_ ;
  std::unique_ptr<StreamSocket> socket_ ;
  std::unique_ptr<std::thread> thread_ ;
  std::uint16_t read_and_write_attempt_count_ ;

  // Callbacks
  ClosedCallback closed_callback_ ;
  ErrorCallback error_callback_ ;

  DISALLOW_COPY_AND_ASSIGN(TcpServerSession) ;
};

#endif  // V8_VM_APPS_HTTP_SERVER_TCP_SERVER_SESSION_H_
