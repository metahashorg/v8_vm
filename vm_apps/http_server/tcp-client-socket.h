// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_HTTP_SERVER_TCP_CLIENT_SOCKET_H_
#define V8_VM_APPS_HTTP_SERVER_TCP_CLIENT_SOCKET_H_

#include <cstdint>
#include <memory>

#include "src/base/macros.h"
#include "vm_apps/http_server/address-list.h"
#include "vm_apps/http_server/stream-socket.h"
#include "vm_apps/http_server/tcp-socket.h"


// A client socket that uses TCP as the transport layer.
class TcpClientSocket : public StreamSocket {
 public:
  // The IP address(es) and port number to connect to.  The TCP socket will try
  // each IP address in the list until it succeeds in establishing a
  // connection.
  // TODO:
  // TCPClientSocket(
  //     const AddressList& addresses,
  //     std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
  //     net::NetLog* net_log,
  //     const net::NetLogSource& source);

  // Adopts the given, connected socket and then acts as if Connect() had been
  // called. This function is used by TCPServerSocket and for testing.
  TcpClientSocket(
      std::unique_ptr<TcpSocket> connected_socket,
      const IPEndPoint& peer_address) ;

  ~TcpClientSocket() override ;

  // Binds the socket to a local IP address and port.
  vv::Error Bind(const IPEndPoint& address) ;

  // StreamSocket implementation.
  vv::Error Connect() override ;
  void Disconnect() override ;
  bool IsConnected() const override ;
  bool IsConnectedAndIdle() const override ;
  vv::Error GetPeerAddress(IPEndPoint* address) const override ;
  vv::Error GetLocalAddress(IPEndPoint* address) const override ;
  // TODO: bool WasEverUsed() const override ;
  // TODO: bool WasAlpnNegotiated() const override;
  // TODO: NextProto GetNegotiatedProtocol() const override;
  // TODO: bool GetSSLInfo(SSLInfo* ssl_info) override;
  // TODO: void GetConnectionAttempts(ConnectionAttempts* out) const override;
  // TODO: void ClearConnectionAttempts() override;
  // TODO: void AddConnectionAttempts(const ConnectionAttempts& attempts) override;
  std::int64_t GetTotalReceivedBytes() const override ;

  // Socket implementation.
  // Multiple outstanding requests are not supported.
  // Full duplex mode (reading and writing at the same time) is supported.
  vv::Error Read(
      char* buf, std::int32_t& buf_len,
      Timeout timeout = kInfiniteTimeout) override ;
  vv::Error Write(
      const char* buf, std::int32_t& buf_len,
      Timeout timeout = kInfiniteTimeout) override ;
  vv::Error SetReceiveBufferSize(std::int32_t size) override ;
  vv::Error SetSendBufferSize(std::int32_t size) override ;

  virtual bool SetKeepAlive(bool enable, int delay) ;
  virtual bool SetNoDelay(bool no_delay) ;

 private:
   // Helper used by Disconnect(), which disconnects minus resetting
   // current_address_index_ and bind_address_.
   void DoDisconnect() ;

  std::unique_ptr<TcpSocket> socket_ ;

  // Local IP address and port we are bound to. Set to NULL if Bind()
  // wasn't called (in that case OS chooses address/port).
  std::unique_ptr<IPEndPoint> bind_address_ ;

  // The list of addresses we should try in order to establish a connection.
  AddressList addresses_ ;

  // Where we are in above list. Set to -1 if uninitialized.
  int current_address_index_ ;

  // This socket was previously disconnected and has not been re-connected.
  bool previously_disconnected_;

  // Total number of bytes received by the socket.
  int64_t total_received_bytes_ ;

  DISALLOW_COPY_AND_ASSIGN(TcpClientSocket) ;
};

#endif  // V8_VM_APPS_HTTP_SERVER_TCP_CLIENT_SOCKET_H_
