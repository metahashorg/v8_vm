// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_HTTP_SERVER_TCP_SOCKET_H_
#define V8_VM_APPS_HTTP_SERVER_TCP_SOCKET_H_

#include "vm_apps/http_server/socket-descriptor.h"

#if defined(V8_OS_WIN)
#include "vm_apps/http_server/tcp-socket-win.h"
#elif defined(V8_OS_POSIX)
#include "vm_apps/http_server/tcp-socket-posix.h"
#endif

// TCPSocket provides a platform-independent interface for TCP sockets.
//
// It is recommended to use TCPClientSocket/TCPServerSocket instead of this
// class, unless a clear separation of client and server socket functionality is
// not suitable for your use case (e.g., a socket needs to be created and bound
// before you know whether it is a client or server socket).
#if defined(V8_OS_WIN)
typedef TcpSocketWin TcpSocket ;
#elif defined(V8_OS_POSIX)
typedef TcpSocketPosix TcpSocket ;
#endif

#endif  // V8_VM_APPS_HTTP_SERVER_TCP_SOCKET_H_
