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

int TcpServerSocket::AdoptSocket(SocketDescriptor socket) {
  return socket_.AdoptUnconnectedSocket(socket) ;
}

vv::Error TcpServerSocket::Listen(const IPEndPoint& address, int backlog) {
  vv::Error result = socket_.Open(address.GetFamily()) ;
  if (result != vv::errOk) {
    printf("ERROR: TcpSocket::Open() returned an error\n") ;
    return result ;
  }

  result = socket_.SetDefaultOptionsForServer() ;
  if (result != vv::errOk) {
    printf("ERROR: TcpSocket::SetDefaultOptionsForServer() "
           "returned an error\n") ;
    socket_.Close() ;
    return result ;
  }

  result = socket_.Bind(address) ;
  if (result != vv::errOk) {
    printf("ERROR: TcpSocket::Bind() returned an error\n") ;
    socket_.Close() ;
    return result ;
  }

  result = socket_.Listen(backlog) ;
  if (result != vv::errOk) {
    printf("ERROR: TcpSocket::Listen() returned an error\n") ;
    socket_.Close() ;
    return result ;
  }

  return vv::errOk ;
}

int TcpServerSocket::GetLocalAddress(IPEndPoint* address) const {
  return socket_.GetLocalAddress(address) ;
}

int TcpServerSocket::Accept(
    std::unique_ptr<StreamSocket>* socket, Timeout timeout) {
  // TODO: DCHECK(socket) ;
  // TODO: DCHECK(!callback.is_null()) ;

  // Make sure the TCPSocket object is destroyed in any case.
  std::unique_ptr<TcpSocket> accepted_socket ;
  IPEndPoint accepted_address ;
  vv::Error result = socket_.Accept(
      &accepted_socket, &accepted_address, timeout) ;
  if (result == vv::errOk) {
    socket->reset(
        new TcpClientSocket(std::move(accepted_socket), accepted_address)) ;
  } else {
    if (result != vv::errTimeout) {
      printf("ERROR: TcpSocket::Accept() returned an error\n") ;
    }
  }

  return result ;
}
