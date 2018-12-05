// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/tcp-server-session.h"

TcpServerSession::~TcpServerSession() {
  if (thread_.get() && thread_->joinable()) {
    thread_->detach() ;
  }

  thread_.reset() ;
  socket_.reset() ;
}

vv::Error TcpServerSession::Start() {
  if (!socket_.get()) {
    printf("ERROR: Socket is invalid\n") ;
    return vv::errObjNotInit ;
  }

  thread_.reset(new std::thread(&TcpServerSession::Run, std::ref(*this))) ;
  return vv::errOk ;
}

vv::Error TcpServerSession::Stop() {
  stop_flag_ = true ;
  if (!thread_.get()) {
    return vv::errObjNotInit ;
  }

  return vv::errOk ;
}

vv::Error TcpServerSession::Wait() {
  if (!thread_.get()) {
    return vv::errObjNotInit ;
  }

  // Wait the thread
  if (thread_->joinable()) {
    thread_->join() ;
  }

  return vv::errOk ;
}

TcpServerSession::TcpServerSession(
  std::unique_ptr<StreamSocket>& socket, Owner* owner)
  : stop_flag_(false), owner_(owner) {
  // TODO: DCHECK(socket.get()) ;
  socket_.swap(socket) ;
}

void TcpServerSession::Run() {
  vv::Error result = Do() ;
  if (owner_) {
    if (V8_ERR_FAILED(result)) {
      owner_->OnSessionError(this, result) ;
    }

    owner_->OnSessionClose(this) ;
  }
}
