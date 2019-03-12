// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/tcp-client-socket.h"

TcpClientSocket::TcpClientSocket(
    std::unique_ptr<TcpSocket> connected_socket,
    const IPEndPoint& peer_address)
  : socket_(std::move(connected_socket)),
    addresses_(AddressList(peer_address)),
    current_address_index_(0),
    total_received_bytes_(0) {
  DCHECK(socket_) ;

  socket_->SetDefaultOptionsForClient() ;
}

TcpClientSocket::~TcpClientSocket() {
  Disconnect() ;
}

Error TcpClientSocket::Bind(const IPEndPoint& address) {
  // TODO:
  return V8_ERROR_CREATE_WITH_MSG(
      errNotImplemented, V8_ERROR_MSG_FUNCTION_FAILED()) ;
}

Error TcpClientSocket::Connect() {
  // TODO:
  return V8_ERROR_CREATE_WITH_MSG(
      errNotImplemented, V8_ERROR_MSG_FUNCTION_FAILED()) ;
}

void TcpClientSocket::Disconnect() {
  DoDisconnect() ;
  current_address_index_ = -1 ;
  bind_address_.reset() ;
}

bool TcpClientSocket::IsConnected() const {
  return socket_->IsConnected() ;
}

bool TcpClientSocket::IsConnectedAndIdle() const {
  return socket_->IsConnectedAndIdle() ;
}

Error TcpClientSocket::GetPeerAddress(IPEndPoint* address) const {
  return socket_->GetPeerAddress(address) ;
}

Error TcpClientSocket::GetLocalAddress(IPEndPoint* address) const {
  DCHECK(address) ;

  if (!socket_->IsValid()) {
    if (bind_address_) {
      *address = *bind_address_ ;
      return errOk ;
    }

    return V8_ERROR_CREATE_WITH_MSG(
        errNetSocketNotConnected, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  return socket_->GetLocalAddress(address) ;
}

std::int64_t TcpClientSocket::GetTotalReceivedBytes() const {
  return total_received_bytes_ ;
}

Error TcpClientSocket::Read(
    char* buf, std::int32_t& buf_len, Timeout timeout) {
  Error result = socket_->Read(buf, buf_len, timeout) ;
  if (V8_ERROR_SUCCESS(result)) {
    total_received_bytes_ += buf_len ;
  } else {
    V8_ERROR_ADD_MSG(result, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  return result ;
}
Error TcpClientSocket::Write(
    const char* buf, std::int32_t& buf_len, Timeout timeout) {
  return socket_->Write(buf, buf_len, timeout) ;
}

Error TcpClientSocket::SetReceiveBufferSize(std::int32_t size) {
  return socket_->SetReceiveBufferSize(size) ;
}
Error TcpClientSocket::SetSendBufferSize(std::int32_t size) {
  return socket_->SetSendBufferSize(size) ;
}

bool TcpClientSocket::SetKeepAlive(bool enable, int delay) {
  return socket_->SetKeepAlive(enable, delay);
}

bool TcpClientSocket::SetNoDelay(bool no_delay) {
  return socket_->SetNoDelay(no_delay);
}

void TcpClientSocket::DoDisconnect() {
  total_received_bytes_ = 0 ;
  // If connecting or already connected, record that the socket has been
  // disconnected.
  previously_disconnected_ = socket_->IsValid() && current_address_index_ >= 0 ;
  socket_->Close() ;
}
