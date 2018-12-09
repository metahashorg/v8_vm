// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_HTTP_SERVER_HTTP_VERSION_H_
#define V8_VM_APPS_HTTP_SERVER_HTTP_VERSION_H_

#include <cstdint>

// Wrapper for an HTTP (major,minor) version pair.
class HttpVersion {
 public:
  // Default constructor (major=0, minor=0).
  HttpVersion() : value_(0) {}

  // Build from unsigned major/minor pair.
  HttpVersion(std::uint16_t major, std::uint16_t minor)
    : value_(major << 16 | minor) {}

  // Major version number.
  uint16_t major_value() const { return value_ >> 16 ; }

  // Minor version number.
  uint16_t minor_value() const { return value_ & 0xffff ; }

  // Overloaded operators:

  bool operator==(const HttpVersion& v) const {
    return value_ == v.value_ ;
  }
  bool operator!=(const HttpVersion& v) const {
    return value_ != v.value_ ;
  }
  bool operator>(const HttpVersion& v) const {
    return value_ > v.value_ ;
  }
  bool operator>=(const HttpVersion& v) const {
    return value_ >= v.value_ ;
  }
  bool operator<(const HttpVersion& v) const {
    return value_ < v.value_ ;
  }
  bool operator<=(const HttpVersion& v) const {
    return value_ <= v.value_ ;
  }

 private:
  std::uint32_t value_ ;  // Packed as <major>:<minor>
};

#endif  // V8_VM_APPS_HTTP_SERVER_HTTP_VERSION_H_
