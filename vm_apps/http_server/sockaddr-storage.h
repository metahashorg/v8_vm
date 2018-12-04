// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_HTTP_SERVER_SOCKADDR_STORAGE_H_
#define V8_VM_APPS_HTTP_SERVER_SOCKADDR_STORAGE_H_

#include "include/v8-vm.h"

#if defined(V8_OS_POSIX)
#include <sys/socket.h>
#include <sys/types.h>
#elif defined(V8_OS_WIN)
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

// Convenience struct for when you need a |struct sockaddr|.
struct SockaddrStorage {
  SockaddrStorage() ;
  SockaddrStorage(const SockaddrStorage& other) ;
  void operator =(const SockaddrStorage& other) ;

  struct sockaddr_storage addr_storage ;
  socklen_t addr_len ;
  struct sockaddr* const addr ;
};

#endif  // V8_VM_APPS_HTTP_SERVER_SOCKADDR_STORAGE_H_
