// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_HTTP_SERVER_WINSOCK_INIT_H_
#define V8_VM_APPS_HTTP_SERVER_WINSOCK_INIT_H_

// Make sure that Winsock is initialized, calling WSAStartup if needed.
void EnsureWinsockInit();

#endif  // V8_VM_APPS_HTTP_SERVER_WINSOCK_INIT_H_
