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

Error TcpServerSession::Start() {
  if (!socket_.get()) {
    return V8_ERROR_CREATE_WITH_MSG(errObjNotInit, "Socket is invalid") ;
  }

  thread_.reset(new std::thread(&TcpServerSession::Run, std::ref(*this))) ;
  return errOk ;
}

Error TcpServerSession::Stop() {
  stop_flag_ = true ;
  if (!thread_.get()) {
    return V8_ERROR_CREATE_WITH_MSG(
        errObjNotInit, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  return errOk ;
}

Error TcpServerSession::Wait() {
  if (!thread_.get()) {
    return V8_ERROR_CREATE_WITH_MSG(
        errObjNotInit, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  // Wait the thread
  if (thread_->joinable()) {
    thread_->join() ;
  }

  return errOk ;
}

void TcpServerSession::SetClosedCallback(const ClosedCallback& callback) {
  closed_callback_ = callback ;
}

void TcpServerSession::SetErrorCallback(const ErrorCallback& callback) {
  error_callback_ = callback ;
}

TcpServerSession::TcpServerSession(std::unique_ptr<StreamSocket>& socket)
  : stop_flag_(false),
    read_and_write_attempt_count_(kDefaultReadAndWriteAttemptCount) {
  // TODO: DCHECK(socket.get()) ;
  socket_.swap(socket) ;
}

Error TcpServerSession::Read(
    char* buf, std::int32_t& buf_len, bool complete_buf) {
  // TODO:
  // DCHECK(socket_) ;
  // DCHECK_NE(nullptr, buf) ;
  // DCHECK_LT(0, buf_len) ;

  // Remember a buffer length and set a result of reading to 0
  std::int32_t local_buf_len = buf_len ;
  buf_len = 0 ;

  // Try to read
  Error result = errOk ;
  for (std::int32_t attempt_count = 0;
       attempt_count < read_and_write_attempt_count_ && local_buf_len > 0;) {
    if (stop_flag_) {
      return V8_ERROR_CREATE_WITH_MSG(errAborted, "Session have been stopped") ;
    }

    std::int32_t circuit_buf_len = local_buf_len ;
    result = socket_->Read(buf, circuit_buf_len, kWaitForReadAndWrite) ;
    if (V8_ERROR_FAILED(result) && result != errTimeout) {
      return V8_ERROR_ADD_MSG(result, V8_ERROR_MSG_FUNCTION_FAILED()) ;
    }

    if (result == errTimeout || circuit_buf_len == 0) {
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

  if (result == errOk && local_buf_len != 0) {
    result = complete_buf ? errIncompleteOperation :
                            wrnIncompleteOperation ;
  }

  return result ;
}

Error TcpServerSession::Write(const char* buf, std::int32_t& buf_len) {
  // TODO:
  // DCHECK(socket_) ;
  // DCHECK_NE(nullptr, buf) ;
  // DCHECK_LT(0, buf_len) ;

  // Remember a buffer length and set a result of writing to 0
  std::int32_t local_buf_len = buf_len ;
  buf_len = 0 ;

  // Try to read
  Error result = errOk ;
  for (std::int32_t attempt_count = 0;
       attempt_count < read_and_write_attempt_count_ && local_buf_len > 0;) {
    if (stop_flag_) {
      return V8_ERROR_CREATE_WITH_MSG(errAborted, "Session have been stopped") ;
    }

    std::int32_t circuit_buf_len = local_buf_len ;
    result = socket_->Write(buf, circuit_buf_len, kWaitForReadAndWrite) ;
    if (V8_ERROR_FAILED(result) && result != errTimeout) {
      return V8_ERROR_ADD_MSG(result, V8_ERROR_MSG_FUNCTION_FAILED()) ;
    }

    if (result == errTimeout || circuit_buf_len == 0) {
      ++attempt_count ;
      continue ;
    }

    attempt_count = 0 ;
    buf += circuit_buf_len ;
    buf_len += circuit_buf_len ;
    local_buf_len -= circuit_buf_len ;
  }

  if (result == errOk && local_buf_len != 0) {
    result = errIncompleteOperation ;
  }

  return result ;
}

void TcpServerSession::Run() {
  Error result = Do() ;
  if (error_callback_ && V8_ERROR_FAILED(result)) {
    error_callback_(this, result) ;
  }

  if (closed_callback_) {
    closed_callback_(this) ;
  }
}
