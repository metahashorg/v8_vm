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

vv::Error TcpServer::Start(
    std::uint16_t port, TcpServerSession::Creator session_creator) {
  if (!port || !session_creator) {
    printf("ERROR: TcpServer::Start() - invalid argument\n") ;
    return vv::errInvalidArgument ;
  }

  session_creator_ = session_creator ;

  ip_endpoint_.reset(new IPEndPoint(IPAddress(0, 0, 0, 0), port)) ;

  vv::Error result = socket_.Listen(*ip_endpoint_.get(), kListenBacklog) ;
  V8_ERR_RETURN_IF_FAILED(result) ;

  thread_.reset(new std::thread(&TcpServer::Run, std::ref(*this))) ;
  return vv::errOk ;
}

vv::Error TcpServer::Stop() {
  stop_flag_ = true ;
  if (!thread_.get()) {
    return vv::errObjNotInit ;
  }

  return vv::errOk ;
}

void TcpServer::OnSessionClose(TcpServerSession* session) {
  std::unique_lock<std::mutex> locker(sessions_lock_) ;
  sessions_.erase(session) ;
  delete session ;
  sessions_cv_.notify_all() ;
}

vv::Error TcpServer::Wait() {
  if (!thread_.get()) {
    return vv::errObjNotInit ;
  }

  // Wait main thread
  thread_->join() ;

  // Wait sessions
  for (;;) {
    std::unique_lock<std::mutex> locker(sessions_lock_) ;
    if (!sessions_.size()) {
      break ;
    }

    vv::Error session_error = (*sessions_.begin())->Stop() ;
    if (V8_ERR_FAILED(session_error)) {
      delete (*sessions_.begin()) ;
      sessions_.erase(sessions_.begin()) ;
      continue ;
    }

    sessions_cv_.wait(locker) ;
  }

  return vv::errOk ;
}

void TcpServer::OnSessionError(TcpServerSession* session, vv::Error error) {}

void TcpServer::Run() {
  while(!stop_flag_) {
    std::unique_ptr<StreamSocket> accepted_socket ;
    vv::Error result = socket_.Accept(&accepted_socket, kWaitForAccept) ;
    if (result == vv::errOk) {
      TcpServerSession* session = session_creator_(accepted_socket, this) ;
      if (session) {
        vv::Error session_error = session->Start() ;
        if (V8_ERR_FAILED(session_error)) {
          delete session ;
        } else {
          std::unique_lock<std::mutex> locker(sessions_lock_) ;
          sessions_.insert(session) ;
        }
      }

      continue ;
    }

    if (result != vv::errTimeout) {
      printf("ERROR: TcpServer::Run() - Accept() returned an error\n") ;
      break ;
    }
  }
}
