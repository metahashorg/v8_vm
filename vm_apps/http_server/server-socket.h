// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_HTTP_SERVER_SERVER_SOCKET_H_
#define V8_VM_APPS_HTTP_SERVER_SERVER_SOCKET_H_

#include <memory>
#include <string>

#include "src/base/macros.h"
#include "vm_apps/http_server/socket-descriptor.h"
#include "vm_apps/utils/app-utils.h"

class IPEndPoint ;
class StreamSocket ;

class ServerSocket {
 public:
  ServerSocket() ;
  virtual ~ServerSocket() ;

  // Binds the socket and starts listening. Destroys the socket to stop
  // listening.
  virtual Error Listen(const IPEndPoint& address, int backlog) = 0 ;

  // Binds the socket with address and port, and starts listening. It expects
  // a valid IPv4 or IPv6 address. Otherwise, it returns errNetAddressInvalid.
  // TODO:
  // virtual Error ListenWithAddressAndPort(
  //     const std::string& address_string, std::uint16_t port, int backlog) ;

  // Gets current address the socket is bound to.
  virtual Error GetLocalAddress(IPEndPoint* address) const = 0 ;

  // Accepts connection.
  virtual Error Accept(
      std::unique_ptr<StreamSocket>* socket,
      Timeout timeout = kInfiniteTimeout) = 0 ;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServerSocket) ;
};

#endif  // V8_VM_APPS_HTTP_SERVER_SERVER_SOCKET_H_
