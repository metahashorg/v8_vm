// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_HTTP_SERVER_ADDRESS_LIST_H_
#define V8_VM_APPS_HTTP_SERVER_ADDRESS_LIST_H_

#include <cstdint>
#include <string>
#include <vector>

#include "vm_apps/http_server/ip-endpoint.h"

struct addrinfo ;
class IPAddress ;

class AddressList {
 public:
  AddressList() ;
  AddressList(const AddressList&) ;
  ~AddressList() ;

  // Creates an address list for a single IP literal.
  explicit AddressList(const IPEndPoint& endpoint) ;

  static AddressList CreateFromIPAddress(
      const IPAddress& address, std::uint16_t port) ;

  static AddressList CreateFromIPAddressList(
      const IPAddressList& addresses, const std::string& canonical_name) ;

  // Copies the data from |head| and the chained list into an AddressList.
  static AddressList CreateFromAddrinfo(const struct addrinfo* head) ;

  // Returns a copy of |list| with port on each element set to |port|.
  static AddressList CopyWithPort(const AddressList& list, std::uint16_t port) ;

  // TODO(szym): Remove all three. http://crbug.com/126134
  const std::string& canonical_name() const { return canonical_name_ ; }

  void set_canonical_name(const std::string& canonical_name) {
    canonical_name_ = canonical_name ;
  }

  // Sets canonical name to the literal of the first IP address on the list.
  void SetDefaultCanonicalName() ;

  using iterator = std::vector<IPEndPoint>::iterator ;
  using const_iterator = std::vector<IPEndPoint>::const_iterator ;

  size_t size() const { return endpoints_.size() ; }
  bool empty() const { return endpoints_.empty() ; }
  void clear() { endpoints_.clear() ; }
  void reserve(size_t count) { endpoints_.reserve(count) ; }
  size_t capacity() const { return endpoints_.capacity() ; }
  IPEndPoint& operator[](size_t index) { return endpoints_[index] ; }
  const IPEndPoint& operator[](size_t index) const
      { return endpoints_[index] ; }
  IPEndPoint& front() { return endpoints_.front() ; }
  const IPEndPoint& front() const { return endpoints_.front() ; }
  IPEndPoint& back() { return endpoints_.back() ; }
  const IPEndPoint& back() const { return endpoints_.back() ; }
  void push_back(const IPEndPoint& val) { endpoints_.push_back(val) ; }

  template <typename InputIt>
  void insert(iterator pos, InputIt first, InputIt last) {
    endpoints_.insert(pos, first, last) ;
  }
  iterator begin() { return endpoints_.begin() ; }
  const_iterator begin() const { return endpoints_.begin() ; }
  iterator end() { return endpoints_.end() ; }
  const_iterator end() const { return endpoints_.end() ; }

  const std::vector<IPEndPoint>& endpoints() const { return endpoints_ ; }
  std::vector<IPEndPoint>& endpoints() { return endpoints_ ; }

 private:
  std::vector<IPEndPoint> endpoints_ ;
  // TODO(szym): Remove. http://crbug.com/126134
  std::string canonical_name_ ;
};

#endif  // V8_VM_APPS_HTTP_SERVER_ADDRESS_LIST_H_
