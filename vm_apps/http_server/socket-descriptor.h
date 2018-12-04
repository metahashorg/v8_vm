// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_HTTP_SERVER_SOCKET_DESCRIPTOR_H_
#define V8_VM_APPS_HTTP_SERVER_SOCKET_DESCRIPTOR_H_

#include "include/v8-vm.h"

#if defined(V8_OS_WIN)
#include <winsock2.h>
#endif  // V8_OS_WIN

#if defined(V8_OS_POSIX)
typedef int SystemErrorCode ;
typedef int SocketDescriptor ;
typedef long Timeout ;
const SocketDescriptor kInvalidSocket = -1 ;
const Timeout kInfiniteTimeout = -1 ;
#elif defined(V8_OS_WIN)
typedef unsigned long SystemErrorCode ;
typedef SOCKET SocketDescriptor ;
typedef DWORD Timeout ;
const SocketDescriptor kInvalidSocket = INVALID_SOCKET ;
const Timeout kInfiniteTimeout = WSA_INFINITE ;
#endif

// Creates  socket. See WSASocket/socket documentation of parameters.
SocketDescriptor CreatePlatformSocket(int family, int type, int protocol) ;

#endif  // V8_VM_APPS_HTTP_SERVER_SOCKET_DESCRIPTOR_H_
