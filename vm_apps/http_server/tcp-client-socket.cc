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
  // TODO: DCHECK(socket_);

  socket_->SetDefaultOptionsForClient() ;
}

TcpClientSocket::~TcpClientSocket() {
  Disconnect() ;
}

vv::Error TcpClientSocket::Bind(const IPEndPoint& address) {
  // TODO:
  return vv::errNotImplemented ;
}

vv::Error TcpClientSocket::Connect() {
  // TODO:
  return vv::errNotImplemented ;
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

vv::Error TcpClientSocket::GetPeerAddress(IPEndPoint* address) const {
  return socket_->GetPeerAddress(address) ;
}

vv::Error TcpClientSocket::GetLocalAddress(IPEndPoint* address) const {
  // TODO: DCHECK(address) ;

  if (!socket_->IsValid()) {
    if (bind_address_) {
      *address = *bind_address_ ;
      return vv::errOk ;
    }

    return vv::errNetSocketNotConnected ;
  }

  return socket_->GetLocalAddress(address) ;
}

std::int64_t TcpClientSocket::GetTotalReceivedBytes() const {
  return total_received_bytes_ ;
}

int TcpClientSocket::Read(char* buf, int& buf_len, Timeout timeout) {
  return vv::errNotImplemented ;
}
int TcpClientSocket::Write(char* buf, int& buf_len, Timeout timeout) {
  return vv::errNotImplemented ;
}

vv::Error TcpClientSocket::SetReceiveBufferSize(std::int32_t size) {
  return socket_->SetReceiveBufferSize(size) ;
}
vv::Error TcpClientSocket::SetSendBufferSize(std::int32_t size) {
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
