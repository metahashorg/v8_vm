// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a convenience header to pull in the platform-specific headers
// that define at least:
//
//     struct addrinfo
//     struct sockaddr*
//     getaddrinfo()
//     freeaddrinfo()
//     AI_*
//     AF_*
//
// Prefer including this file instead of directly writing the #if / #else,
// since it avoids duplicating the platform-specific selections.

#ifndef V8_VM_APPS_HTTP_SERVER_SYS_ADDRINFO_H_
#define V8_VM_APPS_HTTP_SERVER_SYS_ADDRINFO_H_

#include "include/v8-vm.h"

#if defined(V8_OS_WIN)
#include <ws2tcpip.h>
#elif defined(V8_OS_POSIX)
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#endif  // V8_VM_APPS_HTTP_SERVER_SYS_ADDRINFO_H_
