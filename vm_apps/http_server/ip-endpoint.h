// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_HTTP_SERVER_IP_ENDPOINT_H_
#define V8_VM_APPS_HTTP_SERVER_IP_ENDPOINT_H_

#include <string>

#include "include/v8-vm.h"
#include "vm_apps/http_server/address-family.h"
#include "vm_apps/http_server/ip-address.h"
#include "vm_apps/http_server/sys-addrinfo.h"

struct sockaddr ;

// An IPEndPoint represents the address of a transport endpoint:
//  * IP address (either v4 or v6)
//  * Port
class IPEndPoint {
 public:
  IPEndPoint() ;
  ~IPEndPoint() ;
  IPEndPoint(const IPAddress& address, uint16_t port) ;
  IPEndPoint(const IPEndPoint& endpoint) ;

  const IPAddress& address() const { return address_ ; }
  uint16_t port() const { return port_ ; }

  // Returns AddressFamily of the address.
  AddressFamily GetFamily() const ;

  // Returns the sockaddr family of the address, AF_INET or AF_INET6.
  int GetSockAddrFamily() const ;

  // Convert to a provided sockaddr struct.
  // |address| is the sockaddr to copy into.  Should be at least
  //    sizeof(struct sockaddr_storage) bytes.
  // |address_length| is an input/output parameter.  On input, it is the
  //    size of data in |address| available.  On output, it is the size of
  //    the address that was copied into |address|.
  // Returns true on success, false on failure.
  V8_WARN_UNUSED_RESULT
  bool ToSockAddr(struct sockaddr* address, socklen_t* address_length) const ;

  // Convert from a sockaddr struct.
  // |address| is the address.
  // |address_length| is the length of |address|.
  // Returns true on success, false on failure.
  V8_WARN_UNUSED_RESULT
  bool FromSockAddr(const struct sockaddr* address, socklen_t address_length) ;

  // Returns value as a string (e.g. "127.0.0.1:80"). Returns the empty string
  // when |address_| is invalid (the port will be ignored).
  std::string ToString() const ;

  // As above, but without port. Returns the empty string when address_ is
  // invalid.
  std::string ToStringWithoutPort() const ;

  bool operator <(const IPEndPoint& that) const ;
  bool operator ==(const IPEndPoint& that) const ;

 private:
  IPAddress address_ ;
  uint16_t port_ ;
};

#endif  // V8_VM_APPS_HTTP_SERVER_IP_ENDPOINT_H_
