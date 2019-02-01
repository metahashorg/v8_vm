// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/tcp-server.h"

namespace {

const int kListenBacklog = 10 ;
Timeout kWaitForAccept = 1000 ; // in milliseconds

}  // namesoace

TcpServer::TcpServer()
  : session_creator_(nullptr),
    stop_flag_(false) {}

TcpServer::~TcpServer() {
  if (thread_.get() && thread_->joinable()) {
    thread_->detach() ;
  }

  thread_.reset() ;
}

Error TcpServer::Start(
    std::uint16_t port, const TcpServerSession::Creator& session_creator) {
  if (!port || !session_creator) {
    printf("ERROR: TcpServer::Start() - invalid argument\n") ;
    return errInvalidArgument ;
  }

  // Remember callback of a tcp-session creation
  session_creator_ = session_creator ;

  // Create ip-endpoint of tcp-server
  ip_endpoint_.reset(new IPEndPoint(IPAddress(0, 0, 0, 0), port)) ;

  // Create socket and initialize it
  Error result = socket_.Listen(*ip_endpoint_.get(), kListenBacklog) ;
  V8_ERROR_RETURN_IF_FAILED(result) ;

  // Create callbacks on session events
  session_closed_callback_ = std::bind(
      &TcpServer::OnSessionClosed, std::ref(*this), std::placeholders::_1) ;
  session_error_callback_ = std::bind(
      &TcpServer::OnSessionError, std::ref(*this), std::placeholders::_1,
      std::placeholders::_2) ;

  // Start a server thread
  thread_.reset(new std::thread(&TcpServer::Run, std::ref(*this))) ;
  return errOk ;
}

Error TcpServer::Stop() {
  stop_flag_ = true ;
  if (!thread_.get()) {
    return errObjNotInit ;
  }

  return errOk ;
}

Error TcpServer::Wait() {
  if (!thread_.get()) {
    return errObjNotInit ;
  }

  // Wait the thread
  if (thread_->joinable()) {
    thread_->join() ;
  }

  // Wait sessions
  for (;;) {
    std::unique_lock<std::mutex> locker(sessions_lock_) ;
    if (!sessions_.size()) {
      break ;
    }

    Error session_error = (*sessions_.begin())->Stop() ;
    if (V8_ERROR_FAILED(session_error)) {
      delete (*sessions_.begin()) ;
      sessions_.erase(sessions_.begin()) ;
      continue ;
    }

    sessions_cv_.wait(locker) ;
  }

  return errOk ;
}

void TcpServer::OnSessionClosed(TcpServerSession* session) {
  std::unique_lock<std::mutex> locker(sessions_lock_) ;
  sessions_.erase(session) ;
  delete session ;
  sessions_cv_.notify_all() ;
}

void TcpServer::OnSessionError(TcpServerSession* session, Error error) {}

void TcpServer::Run() {
  while(!stop_flag_) {
    std::unique_ptr<StreamSocket> accepted_socket ;
    Error result = socket_.Accept(&accepted_socket, kWaitForAccept) ;
    if (result == errOk) {
      TcpServerSession* session = session_creator_(accepted_socket) ;
      if (session) {
        session->SetClosedCallback(session_closed_callback_) ;
        session->SetErrorCallback(session_error_callback_) ;
        Error session_error = session->Start() ;
        if (V8_ERROR_FAILED(session_error)) {
          delete session ;
        } else {
          std::unique_lock<std::mutex> locker(sessions_lock_) ;
          sessions_.insert(session) ;
        }
      }

      continue ;
    }

    if (result != errTimeout) {
      printf("ERROR: TcpServer::Run() - Accept() returned an error\n") ;
    }
  }
}
