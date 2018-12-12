// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_HTTP_SERVER_TCP_SOCKET_WIN_H_
#define V8_VM_APPS_HTTP_SERVER_TCP_SOCKET_WIN_H_

#include <winsock2.h>

#include "include/v8-vm.h"
#include "src/base/macros.h"
#include "vm_apps/http_server/ip-endpoint.h"
#include "vm_apps/http_server/socket-descriptor.h"
#include "vm_apps/utils/app-utils.h"

class TcpSocketWin {
 public:
  TcpSocketWin() ;
  virtual ~TcpSocketWin() ;

  // Opens the socket.
  // Returns a net error code.
  vv::Error Open(AddressFamily family) ;

  // Takes ownership of |socket|, which is known to already be connected to the
  // given peer address. However, peer address may be the empty address, for
  // compatibility. The given peer address will be returned by GetPeerAddress.
  vv::Error AdoptConnectedSocket(
      SocketDescriptor socket, const IPEndPoint& peer_address) ;
  // Takes ownership of |socket|, which may or may not be open, bound, or
  // listening. The caller must determine the state of the socket based on its
  // provenance and act accordingly. The socket may have connections waiting
  // to be accepted, but must not be actually connected.
  vv::Error AdoptUnconnectedSocket(SocketDescriptor socket) ;

  // Binds this socket to |address|. This is generally only used on a server.
  // Should be called after Open(). Returns a net error code.
  vv::Error Bind(const IPEndPoint& address) ;

  // Put this socket on listen state with the given |backlog|.
  // Returns a net error code.
  vv::Error Listen(int backlog) ;

  // Accepts incoming connection.
  // Returns a net error code.
  vv::Error Accept(
      std::unique_ptr<TcpSocketWin>* socket, IPEndPoint* address,
      Timeout timeout = kInfiniteTimeout) ;

  bool IsConnected() const ;
  bool IsConnectedAndIdle() const ;

  // Full duplex mode (reading and writing at the same time) is supported.

  // Reads from the socket.
  // Returns a net error code.
  vv::Error Read(
      char* buf, std::int32_t& buf_len, Timeout timeout = kInfiniteTimeout) ;

  // Writes to the socket.
  // Returns a net error code.
  vv::Error Write(
      const char* buf, std::int32_t& buf_len,
      Timeout timeout = kInfiniteTimeout) ;

  // Copies the local tcp address into |address| and returns a net error code.
  vv::Error GetLocalAddress(IPEndPoint* address) const ;

  // Copies the remote tcp code into |address| and returns a net error code.
  vv::Error GetPeerAddress(IPEndPoint* address) const ;

  // Sets various socket options.
  // The commonly used options for server listening sockets:
  // - SetExclusiveAddrUse().
  vv::Error SetDefaultOptionsForServer() ;
  // The commonly used options for client sockets and accepted sockets:
  // - SetNoDelay(true);
  // - SetKeepAlive(true, 45).
  void SetDefaultOptionsForClient() ;
  vv::Error SetExclusiveAddrUse() ;
  vv::Error SetReceiveBufferSize(std::int32_t size) ;
  vv::Error SetSendBufferSize(std::int32_t size) ;
  bool SetKeepAlive(bool enable, int delay) ;
  bool SetNoDelay(bool no_delay) ;

  // Closes the socket.
  void Close() ;

  bool IsValid() const { return socket_ != INVALID_SOCKET ; }

 private:
  SOCKET socket_ ;
  HANDLE accept_event_ ;
  HANDLE read_event_ ;
  HANDLE write_event_ ;

  // The various states that the socket could be in.
  bool waiting_connect_ ;
  bool waiting_read_ ;
  bool waiting_write_ ;

  std::unique_ptr<IPEndPoint> peer_address_ ;

  DISALLOW_COPY_AND_ASSIGN(TcpSocketWin) ;
};

#endif  // V8_VM_APPS_HTTP_SERVER_TCP_SOCKET_WIN_H_
