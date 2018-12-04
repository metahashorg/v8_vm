// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_SERVER_SOCKET_H_
#define NET_SOCKET_TCP_SERVER_SOCKET_H_

#include <memory>

#include "src/base/macros.h"
#include "vm_apps/http_server/ip-endpoint.h"
#include "vm_apps/http_server/server-socket.h"
#include "vm_apps/http_server/socket-descriptor.h"
#include "vm_apps/http_server/tcp-socket.h"

// A server socket that uses TCP as the transport layer.
class TcpServerSocket : public ServerSocket {
 public:
  TcpServerSocket() ;
  ~TcpServerSocket() override ;

  // Takes ownership of |socket|, which has been opened, but may or may not be
  // bound or listening. The caller must determine this based on the provenance
  // of the socket and act accordingly. The socket may have connections waiting
  // to be accepted, but must not be actually connected.
  vv::Error AdoptSocket(SocketDescriptor socket) ;

  // ServerSocket implementation.
  vv::Error Listen(const IPEndPoint& address, int backlog) override ;
  vv::Error GetLocalAddress(IPEndPoint* address) const override ;
  vv::Error Accept(
      std::unique_ptr<StreamSocket>* socket,
      Timeout timeout = kInfiniteTimeout) override ;

 private:
  TcpSocket socket_ ;

  DISALLOW_COPY_AND_ASSIGN(TcpServerSocket) ;
};

#endif  // NET_SOCKET_TCP_SERVER_SOCKET_H_
