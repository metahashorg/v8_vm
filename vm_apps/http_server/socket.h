// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_HTTP_SERVER_SOCKET_H_
#define V8_VM_APPS_HTTP_SERVER_SOCKET_H_

#include <cstdint>

#include "vm_apps/http_server/socket-descriptor.h"
#include "vm_apps/utils/app-utils.h"

class IOBuffer ;

// Represents a read/write socket.
class Socket {
 public:
  virtual ~Socket() {}

  // Reads data, up to |buf_len| bytes, from the socket.  The number of bytes
  // read is returned in |buf_len|, or an error is returned upon failure.
  // errNetSocketNotConnected should be returned if the socket is not currently
  // connected.  Zero is returned once to indicate end-of-file; the return value
  // of subsequent calls is undefined, and may be OS dependent. errNetIOPending
  // is returned if the operation could not be completed synchronously, in which
  // case the result will be passed to the callback when available. If the
  // operation is not completed immediately, the socket acquires a reference to
  // the provided buffer until the callback is invoked or the socket is
  // closed.  If the socket is Disconnected before the read completes, the
  // callback will not be invoked. |timeout| is in milliseconds.
  virtual vv::Error Read(
      char* buf, int& buf_len, Timeout timeout = kInfiniteTimeout) = 0 ;

  // Writes data, up to |buf_len| bytes, to the socket.  Note: data may be
  // written partially. The number of bytes written is returned in |buf_len|, or
  // an error is returned upon failure. errNetSocketNotConnected should be
  // returned if the socket is not currently connected. The return value when
  // the connection is closed is undefined, and may be OS dependent.
  // errNetIOPending is returned if the operation could not be completed
  // synchronously, in which case the result will be passed to the callback when
  // available. If the operation is not completed immediately, the socket
  // acquires a reference to the provided buffer until the callback is invoked
  // or the socket is closed. Implementations of this method should not modify
  // the contents of the actual buffer that is written to the socket. If the
  // socket is Disconnected before the write completes, the callback will not be
  // invoked. |timeout| is in milliseconds.
  virtual vv::Error Write(
      char* buf, int& buf_len, Timeout timeout = kInfiniteTimeout) = 0 ;

  // Set the receive buffer size (in bytes) for the socket.
  // Note: changing this value can affect the TCP window size on some platforms.
  // Returns a net error code.
  virtual vv::Error SetReceiveBufferSize(std::int32_t size) = 0 ;

  // Set the send buffer size (in bytes) for the socket.
  // Note: changing this value can affect the TCP window size on some platforms.
  // Returns a net error code.
  virtual vv::Error SetSendBufferSize(std::int32_t size) = 0 ;
};

#endif  // V8_VM_APPS_HTTP_SERVER_SOCKET_H_
