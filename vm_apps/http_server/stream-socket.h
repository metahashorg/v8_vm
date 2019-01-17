// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_HTTP_SERVER_STREAM_SOCKET_H_
#define V8_VM_APPS_HTTP_SERVER_STREAM_SOCKET_H_

#include <cstdint>

#include "vm_apps/http_server/socket.h"

class IPEndPoint ;
// TODO: class SSLInfo ;

class StreamSocket : public Socket {
 public:
  ~StreamSocket() override {}

  // Called to establish a connection. Returns errOK if the connection could be
  // established synchronously.  Otherwise, errNetIOPending is returned and the
  // given callback will run asynchronously when the connection is established
  // or when an error occurs.  The result is some other error code if the
  // connection could not be established.
  //
  // The socket's Read and Write methods may not be called until Connect
  // succeeds.
  //
  // It is valid to call Connect on an already connected socket, in which case
  // OK is simply returned.
  //
  // Connect may also be called again after a call to the Disconnect method.
  //
  virtual Error Connect() = 0 ;

  // Called to disconnect a socket.  Does nothing if the socket is already
  // disconnected.  After calling Disconnect it is possible to call Connect
  // again to establish a new connection.
  //
  // If IO (Connect, Read, or Write) is pending when the socket is
  // disconnected, the pending IO is cancelled, and the completion callback
  // will not be called.
  virtual void Disconnect() = 0 ;

  // Called to test if the connection is still alive.  Returns false if a
  // connection wasn't established or the connection is dead.  True is returned
  // if the connection was terminated, but there is unread data in the incoming
  // buffer.
  virtual bool IsConnected() const = 0 ;

  // Called to test if the connection is still alive and idle.  Returns false
  // if a connection wasn't established, the connection is dead, or there is
  // unread data in the incoming buffer.
  virtual bool IsConnectedAndIdle() const = 0 ;

  // Copies the peer address to |address| and returns a network error code.
  // errNetSocketNotConnected will be returned if the socket is not connected.
  virtual Error GetPeerAddress(IPEndPoint* address) const = 0 ;

  // Copies the local address to |address| and returns a network error code.
  // errNetSocketNotConnected will be returned if the socket is not bound.
  virtual Error GetLocalAddress(IPEndPoint* address) const = 0 ;

  // Returns true if the socket ever had any reads or writes.  StreamSockets
  // layered on top of transport sockets should return if their own Read() or
  // Write() methods had been called, not the underlying transport's.
  // TODO: virtual bool WasEverUsed() const = 0 ;

  // Returns true if ALPN was negotiated during the connection of this socket.
  // TODO: virtual bool WasAlpnNegotiated() const = 0 ;

  // Returns the protocol negotiated via ALPN for this socket, or
  // kProtoUnknown will be returned if ALPN is not applicable.
  // TODO: virtual NextProto GetNegotiatedProtocol() const = 0 ;

  // Gets the SSL connection information of the socket.  Returns false if
  // SSL was not used by this socket.
  // TODO: virtual bool GetSSLInfo(SSLInfo* ssl_info) = 0 ;

  // Overwrites |out| with the connection attempts made in the process of
  // connecting this socket.
  // TODO: virtual void GetConnectionAttempts(ConnectionAttempts* out) const = 0 ;

  // Clears the socket's list of connection attempts.
  // TODO: virtual void ClearConnectionAttempts() = 0 ;

  // Adds |attempts| to the socket's list of connection attempts.
  // TODO: virtual void AddConnectionAttempts(const ConnectionAttempts& attempts) = 0 ;

  // Returns the total number of number bytes read by the socket. This only
  // counts the payload bytes. Transport headers are not counted. Returns
  // 0 if the socket does not implement the function. The count is reset when
  // Disconnect() is called.
  virtual std::int64_t GetTotalReceivedBytes() const = 0 ;
};

#endif  // V8_VM_APPS_HTTP_SERVER_STREAM_SOCKET_H_
