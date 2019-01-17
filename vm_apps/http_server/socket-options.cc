// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/socket-options.h"

#include <cerrno>

#include "include/v8-vm.h"
#include "vm_apps/http_server/net-errors.h"

#if defined(V8_OS_POSIX)
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#elif defined(V8_OS_WIN)
#include <winsock2.h>
#endif

Error SetTCPNoDelay(SocketDescriptor fd, bool no_delay) {
#if defined(V8_OS_POSIX)
  int on = no_delay ? 1 : 0 ;
#elif defined(V8_OS_WIN)
  BOOL on = no_delay ? TRUE : FALSE ;
#endif
  int rv = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                      reinterpret_cast<const char*>(&on), sizeof(on)) ;
  return rv == -1 ? MapSystemError(errno) : errOk ;
}

Error SetReuseAddr(SocketDescriptor fd, bool reuse) {
// SO_REUSEADDR is useful for server sockets to bind to a recently unbound
// port. When a socket is closed, the end point changes its state to TIME_WAIT
// and wait for 2 MSL (maximum segment lifetime) to ensure the remote peer
// acknowledges its closure. For server sockets, it is usually safe to
// bind to a TIME_WAIT end point immediately, which is a widely adopted
// behavior.
//
// Note that on *nix, SO_REUSEADDR does not enable the socket (which can be
// either TCP or UDP) to bind to an end point that is already bound by another
// socket. To do that one must set SO_REUSEPORT instead. This option is not
// provided on Linux prior to 3.9.
//
// SO_REUSEPORT is provided in MacOS X and iOS.
#if defined(V8_OS_POSIX)
  int boolean_value = reuse ? 1 : 0 ;
#elif defined(V8_OS_WIN)
  BOOL boolean_value = reuse ? TRUE : FALSE ;
#endif
  int rv = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                      reinterpret_cast<const char*>(&boolean_value),
                      sizeof(boolean_value)) ;
  return rv == -1 ? MapSystemError(errno) : errOk ;
}

Error SetSocketReceiveBufferSize(SocketDescriptor fd, int32_t size) {
  int rv = setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
                      reinterpret_cast<const char*>(&size), sizeof(size)) ;
#if defined(V8_OS_POSIX)
  int os_error = errno ;
#elif defined(V8_OS_WIN)
  int os_error = WSAGetLastError() ;
#endif
  Error net_error = (rv == -1) ? MapSystemError(os_error) : errOk ;
  if (!!rv) {
    printf("ERROR: Could not set socket receive buffer size\n") ;
  }

  return net_error ;
}

Error SetSocketSendBufferSize(SocketDescriptor fd, int32_t size) {
  int rv = setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
                      reinterpret_cast<const char*>(&size), sizeof(size)) ;
#if defined(V8_OS_POSIX)
  int os_error = errno ;
#elif defined(V8_OS_WIN)
  int os_error = WSAGetLastError() ;
#endif
  Error net_error = (rv == -1) ? MapSystemError(os_error) : errOk ;
  if (!!rv) {
    printf("ERROR:Could not set socket sent buffer size\n") ;
  }

  return net_error;
}
