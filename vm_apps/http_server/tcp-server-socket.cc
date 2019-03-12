// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/tcp-server-socket.h"

#include "vm_apps/http_server/socket-descriptor.h"
#include "vm_apps/http_server/tcp-client-socket.h"

TcpServerSocket::TcpServerSocket() {}

TcpServerSocket::~TcpServerSocket() {}

Error TcpServerSocket::AdoptSocket(SocketDescriptor socket) {
  return socket_.AdoptUnconnectedSocket(socket) ;
}

Error TcpServerSocket::Listen(const IPEndPoint& address, int backlog) {
  Error result = socket_.Open(address.GetFamily()) ;
  if (result != errOk) {
    return V8_ERROR_ADD_MSG(result, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  result = socket_.SetDefaultOptionsForServer() ;
  if (result != errOk) {
    socket_.Close() ;
    return V8_ERROR_ADD_MSG(result, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  result = socket_.Bind(address) ;
  if (result != errOk) {
    socket_.Close() ;
    return V8_ERROR_ADD_MSG(result, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  result = socket_.Listen(backlog) ;
  if (result != errOk) {
    socket_.Close() ;
    return V8_ERROR_ADD_MSG(result, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  return errOk ;
}

Error TcpServerSocket::GetLocalAddress(IPEndPoint* address) const {
  return socket_.GetLocalAddress(address) ;
}

Error TcpServerSocket::Accept(
    std::unique_ptr<StreamSocket>* socket, Timeout timeout) {
  DCHECK(socket) ;

  // Make sure the TCPSocket object is destroyed in any case.
  std::unique_ptr<TcpSocket> accepted_socket ;
  IPEndPoint accepted_address ;
  Error result = socket_.Accept(
      &accepted_socket, &accepted_address, timeout) ;
  if (result == errOk) {
    socket->reset(
        new TcpClientSocket(std::move(accepted_socket), accepted_address)) ;
  } else {
    if (result != errTimeout) {
      V8_ERROR_ADD_MSG(result, V8_ERROR_MSG_FUNCTION_FAILED()) ;
    }
  }

  return result ;
}
