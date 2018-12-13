// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/tcp-server-session.h"

namespace {

std::int32_t kDefaultReadAndWriteAttemptCount = 10 ;
Timeout kWaitForReadAndWrite = 1000 ; // in milliseconds

}

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

void TcpServerSession::SetClosedCallback(ClosedCallback callback) {
  closed_callback_ = callback ;
}

void TcpServerSession::SetErrorCallback(ErrorCallback callback) {
  error_callback_ = callback ;
}

TcpServerSession::TcpServerSession(std::unique_ptr<StreamSocket>& socket)
  : stop_flag_(false),
    read_and_write_attempt_count_(kDefaultReadAndWriteAttemptCount) {
  // TODO: DCHECK(socket.get()) ;
  socket_.swap(socket) ;
}

vv::Error TcpServerSession::Read(
    char* buf, std::int32_t& buf_len, bool complete_buf) {
  // TODO:
  // DCHECK(socket_) ;
  // DCHECK_NE(nullptr, buf) ;
  // DCHECK_LT(0, buf_len) ;

  // Remember a buffer length and set a result of reading to 0
  std::int32_t local_buf_len = buf_len ;
  buf_len = 0 ;

  // Try to read
  vv::Error result = vv::errOk ;
  for (std::int32_t attempt_count = 0;
       attempt_count < read_and_write_attempt_count_ && local_buf_len > 0;) {
    if (stop_flag_) {
      printf("ERROR: TcpServerSession have been stopped\n") ;
      return vv::errAborted ;
    }

    std::int32_t circuit_buf_len = local_buf_len ;
    result = socket_->Read(buf, circuit_buf_len, kWaitForReadAndWrite) ;
    if (V8_ERR_FAILED(result) && result != vv::errTimeout) {
      printf("ERROR: socket_->Read() returned an error\n") ;
      return result ;
    }

    if (result == vv::errTimeout || circuit_buf_len == 0) {
      ++attempt_count ;
      continue ;
    }

    attempt_count = 0 ;
    buf += circuit_buf_len ;
    buf_len += circuit_buf_len ;
    local_buf_len -= circuit_buf_len ;

    // If we don't need to complete a buffer
    // we'll stop reading after first success
    if (!complete_buf){
      break ;
    }
  }

  if (result == vv::errOk && local_buf_len != 0) {
    result = complete_buf ? vv::errIncompleteOperation :
                            vv::wrnIncompleteOperation ;
  }

  return result ;
}

vv::Error TcpServerSession::Write(const char* buf, std::int32_t& buf_len) {
  // TODO:
  // DCHECK(socket_) ;
  // DCHECK_NE(nullptr, buf) ;
  // DCHECK_LT(0, buf_len) ;

  // Remember a buffer length and set a result of writing to 0
  std::int32_t local_buf_len = buf_len ;
  buf_len = 0 ;

  // Try to read
  vv::Error result = vv::errOk ;
  for (std::int32_t attempt_count = 0;
       attempt_count < read_and_write_attempt_count_ && local_buf_len > 0;) {
    if (stop_flag_) {
      printf("ERROR: TcpServerSession have been stopped\n") ;
      return vv::errAborted ;
    }

    std::int32_t circuit_buf_len = local_buf_len ;
    result = socket_->Write(buf, circuit_buf_len, kWaitForReadAndWrite) ;
    if (V8_ERR_FAILED(result) && result != vv::errTimeout) {
      printf("ERROR: socket_->Write() returned an error\n") ;
      return result ;
    }

    if (result == vv::errTimeout || circuit_buf_len == 0) {
      ++attempt_count ;
      continue ;
    }

    attempt_count = 0 ;
    buf += circuit_buf_len ;
    buf_len += circuit_buf_len ;
    local_buf_len -= circuit_buf_len ;
  }

  if (result == vv::errOk && local_buf_len != 0) {
    result = vv::errIncompleteOperation ;
  }

  return result ;
}

void TcpServerSession::Run() {
  vv::Error result = Do() ;
  if (error_callback_ && V8_ERR_FAILED(result)) {
    error_callback_(this, result) ;
  }

  if (closed_callback_) {
    closed_callback_(this) ;
  }
}
