// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_HTTP_SERVER_SOCKET_POSIX_H_
#define V8_VM_APPS_HTTP_SERVER_SOCKET_POSIX_H_

#include <memory>

#include "src/base/macros.h"
#include "vm_apps/http_server/sockaddr-storage.h"
#include "vm_apps/http_server/socket-descriptor.h"
#include "vm_apps/utils/app-utils.h"

// Socket class to provide asynchronous read/write operations on top of the
// posix socket api. It supports AF_INET, AF_INET6, and AF_UNIX addresses.
class SocketPosix {
 public:
  SocketPosix() ;
  ~SocketPosix() ;

  // Opens a socket and returns net::OK if |address_family| is AF_INET, AF_INET6
  // or AF_UNIX. Otherwise, it does DCHECK() and returns a net error.
  Error Open(int address_family);

  // Takes ownership of |socket|, which is known to already be connected to the
  // given peer address.
  Error AdoptConnectedSocket(
      SocketDescriptor socket, const SockaddrStorage& peer_address) ;
  // Takes ownership of |socket|, which may or may not be open, bound, or
  // listening. The caller must determine the state of the socket based on its
  // provenance and act accordingly. The socket may have connections waiting
  // to be accepted, but must not be actually connected.
  Error AdoptUnconnectedSocket(SocketDescriptor socket) ;

  Error Bind(const SockaddrStorage& address) ;

  Error Listen(int backlog) ;
  Error Accept(
      std::unique_ptr<SocketPosix>* socket,
      Timeout timeout = kInfiniteTimeout) ;

  bool IsConnected() const ;
  bool IsConnectedAndIdle() const ;

  // Full duplex mode (reading and writing at the same time) is supported.

  // Reads from the socket.
  // Returns a net error code.
  Error Read(
      char* buf, std::int32_t& buf_len, Timeout timeout = kInfiniteTimeout) ;

  // Writes to the socket.
  // Returns a net error code.
  Error Write(
      const char* buf, std::int32_t& buf_len,
      Timeout timeout = kInfiniteTimeout) ;

  Error GetLocalAddress(SockaddrStorage* address) const ;
  Error GetPeerAddress(SockaddrStorage* address) const ;
  void SetPeerAddress(const SockaddrStorage& address) ;
  // Returns true if peer address has been set regardless of socket state.
  bool HasPeerAddress() const ;

  void Close() ;

  SocketDescriptor socket_fd() const { return socket_fd_ ; }

 private:
  SocketDescriptor socket_fd_ ;

  // A connect operation is pending. In this case, |write_callback_| needs to be
  // called when connect is complete.
  bool waiting_connect_ ;

  std::unique_ptr<SockaddrStorage> peer_address_ ;

  DISALLOW_COPY_AND_ASSIGN(SocketPosix) ;
};

#endif  // V8_VM_APPS_HTTP_SERVER_SOCKET_POSIX_H_
