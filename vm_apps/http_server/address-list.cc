// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/address-list.h"

#include "vm_apps/http_server/sys-addrinfo.h"

AddressList::AddressList() {}

AddressList::AddressList(const AddressList&) = default ;

AddressList::~AddressList() {}

AddressList::AddressList(const IPEndPoint& endpoint) {
  push_back(endpoint) ;
}

// static
AddressList AddressList::CreateFromIPAddress(
    const IPAddress& address, std::uint16_t port) {
  return AddressList(IPEndPoint(address, port)) ;
}

// static
AddressList AddressList::CreateFromIPAddressList(
    const IPAddressList& addresses, const std::string& canonical_name) {
  AddressList list ;
  list.set_canonical_name(canonical_name) ;
  for (IPAddressList::const_iterator iter = addresses.begin();
       iter != addresses.end(); ++iter) {
    list.push_back(IPEndPoint(*iter, 0)) ;
  }

  return list ;
}

// static
AddressList AddressList::CreateFromAddrinfo(const struct addrinfo* head) {
  DCHECK(head) ;
  AddressList list ;
  if (head->ai_canonname) {
    list.set_canonical_name(std::string(head->ai_canonname)) ;
  }

  for (const struct addrinfo* ai = head; ai; ai = ai->ai_next) {
    IPEndPoint ipe ;
    // NOTE: Ignoring non-INET* families.
    if (ipe.FromSockAddr(ai->ai_addr, static_cast<socklen_t>(ai->ai_addrlen))) {
      list.push_back(ipe) ;
    } else{
      V8_LOG_WRN(
          wrnNetUnknownAddressFamily, "Unknown family found in addrinfo") ;
    }
  }

  return list ;
}

// static
AddressList AddressList::CopyWithPort(
    const AddressList& list, std::uint16_t port) {
  AddressList out ;
  out.set_canonical_name(list.canonical_name()) ;
  for (size_t i = 0; i < list.size(); ++i) {
    out.push_back(IPEndPoint(list[i].address(), port)) ;
  }

  return out ;
}

// TODO:
// void AddressList::SetDefaultCanonicalName() {
//   DCHECK(!empty());
//   set_canonical_name(front().ToStringWithoutPort());
// }
