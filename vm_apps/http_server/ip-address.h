// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_HTTP_SERVER_IP_ADDRESS_H_
#define V8_VM_APPS_HTTP_SERVER_IP_ADDRESS_H_

#include <array>
#include <string>
#include <vector>

#include "include/v8-vm.h"
#include "src/base/logging.h"

// Helper class to represent the sequence of bytes in an IP address.
// A vector<uint8_t> would be simpler but incurs heap allocation, so
// IPAddressBytes uses a fixed size array.
class IPAddressBytes {
 public:
  IPAddressBytes() ;
  IPAddressBytes(const std::uint8_t* data, size_t data_len) ;
  IPAddressBytes(const IPAddressBytes& other) ;
  ~IPAddressBytes() ;

  // Copies |data_len| elements from |data| into this object.
  void Assign(const std::uint8_t* data, size_t data_len) ;

  // Returns the number of elements in the underlying array.
  size_t size() const { return size_ ; }

  // Sets the size to be |size|. Does not actually change the size
  // of the underlying array or zero-initialize the bytes.
  void Resize(size_t size) {
    DCHECK_LE(size, 16u) ;
    size_ = static_cast<std::uint8_t>(size) ;
  }

  // Returns true if the underlying array is empty.
  bool empty() const { return size_ == 0 ; }

  // Returns a pointer to the underlying array of bytes.
  const std::uint8_t* data() const { return bytes_.data() ; }
  std::uint8_t* data() { return bytes_.data() ; }

  // Returns a pointer to the first element.
  const std::uint8_t* begin() const { return data() ; }
  std::uint8_t* begin() { return data() ; }

  // Returns a pointer past the last element.
  const std::uint8_t* end() const { return data() + size_ ; }
  std::uint8_t* end() { return data() + size_ ; }

  // Returns a reference to the last element.
  std::uint8_t& back() {
    DCHECK(!empty()) ;
    return bytes_[size_ - 1] ;
  }
  const uint8_t& back() const {
    DCHECK(!empty()) ;
    return bytes_[size_ - 1] ;
  }

  // Appends |val| to the end and increments the size.
  void push_back(std::uint8_t val) {
    DCHECK_GT(16, size_) ;
    bytes_[size_++] = val ;
  }

  // Returns a reference to the byte at index |pos|.
  std::uint8_t& operator[](size_t pos) {
    DCHECK_LT(pos, size_) ;
    return bytes_[pos] ;
  }
  const std::uint8_t& operator[](size_t pos) const {
    DCHECK_LT(pos, size_) ;
    return bytes_[pos] ;
  }

  bool operator <(const IPAddressBytes& other) const ;
  bool operator !=(const IPAddressBytes& other) const ;
  bool operator ==(const IPAddressBytes& other) const ;

 private:
  // Underlying sequence of bytes
  std::array<std::uint8_t, 16> bytes_ ;

  // Number of elements in |bytes_|. Should be either kIPv4AddressSize
  // or kIPv6AddressSize or 0.
  std::uint8_t size_ ;
};

class IPAddress {
 public:
  enum : size_t { kIPv4AddressSize = 4, kIPv6AddressSize = 16 };

  // Creates a zero-sized, invalid address.
  IPAddress() ;

  IPAddress(const IPAddress& other) ;

  // Copies the input address to |ip_address_|.
  explicit IPAddress(const IPAddressBytes& address) ;

  // Copies the input address to |ip_address_|. The input is expected to be in
  // network byte order.
  template <size_t N>
  IPAddress(const std::uint8_t(&address)[N])
      : IPAddress(address, N) {}

  // Copies the input address to |ip_address_| taking an additional length
  // parameter. The input is expected to be in network byte order.
  IPAddress(const std::uint8_t* address, size_t address_len) ;

  // Initializes |ip_address_| from the 4 bX bytes to form an IPv4 address.
  // The bytes are expected to be in network byte order.
  IPAddress(
      std::uint8_t b0, std::uint8_t b1, std::uint8_t b2, std::uint8_t b3) ;

  // Initializes |ip_address_| from the 16 bX bytes to form an IPv6 address.
  // The bytes are expected to be in network byte order.
  IPAddress(std::uint8_t b0,
            std::uint8_t b1,
            std::uint8_t b2,
            std::uint8_t b3,
            std::uint8_t b4,
            std::uint8_t b5,
            std::uint8_t b6,
            std::uint8_t b7,
            std::uint8_t b8,
            std::uint8_t b9,
            std::uint8_t b10,
            std::uint8_t b11,
            std::uint8_t b12,
            std::uint8_t b13,
            std::uint8_t b14,
            std::uint8_t b15) ;

  ~IPAddress() ;

  // Returns true if the IP has |kIPv4AddressSize| elements.
  bool IsIPv4() const ;

  // Returns true if the IP has |kIPv6AddressSize| elements.
  bool IsIPv6() const ;

  // Returns true if the IP is either an IPv4 or IPv6 address. This function
  // only checks the address length.
  bool IsValid() const ;

  // Returns true if the IP is in a range reserved by the IANA.
  // Works with both IPv4 and IPv6 addresses, and only compares against a given
  // protocols's reserved ranges.
  bool IsReserved() const ;

  // Returns true if the IP is "zero" (e.g. the 0.0.0.0 IPv4 address).
  bool IsZero() const ;

  // Returns true if |ip_address_| is an IPv4-mapped IPv6 address.
  bool IsIPv4MappedIPv6() const ;

  // The size in bytes of |ip_address_|.
  size_t size() const { return ip_address_.size() ; }

  // Returns true if the IP is an empty, zero-sized (invalid) address.
  bool empty() const { return ip_address_.empty() ; }

  // Returns the canonical string representation of an IP address.
  // For example: "192.168.0.1" or "::1". Returns the empty string when
  // |ip_address_| is invalid.
  std::string ToString() const ;

  // Parses an IP address literal (either IPv4 or IPv6) to its numeric value.
  // Returns true on success and fills |ip_address_| with the numeric value.
  V8_WARN_UNUSED_RESULT
  bool AssignFromIPLiteral(const std::string& ip_literal) ;

  // Returns the underlying bytes.
  const IPAddressBytes& bytes() const { return ip_address_ ; }

  // Copies the bytes to a new vector. Generally callers should be using
  // |bytes()| and the IPAddressBytes abstraction. This method is provided as a
  // convenience for call sites that existed prior to the introduction of
  // IPAddressBytes.
  std::vector<std::uint8_t> CopyBytesToVector() const ;

  // Returns an IPAddress instance representing the 127.0.0.1 address.
  static IPAddress IPv4Localhost() ;

  // Returns an IPAddress instance representing the ::1 address.
  static IPAddress IPv6Localhost() ;

  // Returns an IPAddress made up of |num_zero_bytes| zeros.
  static IPAddress AllZeros(size_t num_zero_bytes) ;

  // Returns an IPAddress instance representing the 0.0.0.0 address.
  static IPAddress IPv4AllZeros() ;

  // Returns an IPAddress instance representing the :: address.
  static IPAddress IPv6AllZeros() ;

  bool operator ==(const IPAddress& that) const ;
  bool operator !=(const IPAddress& that) const ;
  bool operator <(const IPAddress& that) const ;

 private:
  IPAddressBytes ip_address_ ;

  // This class is copyable and assignable.
};

using IPAddressList = std::vector<IPAddress> ;

// Returns the canonical string representation of an IP address along with its
// port. For example: "192.168.0.1:99" or "[::1]:80".
std::string IPAddressToStringWithPort(
    const IPAddress& address, std::uint16_t port) ;

// Returns the address as a sequence of bytes in network-byte-order.
std::string IPAddressToPackedString(const IPAddress& address) ;

// Converts an IPv4 address to an IPv4-mapped IPv6 address.
// For example 192.168.0.1 would be converted to ::ffff:192.168.0.1.
// TODO:
// IPAddress ConvertIPv4ToIPv4MappedIPv6(const IPAddress& address) ;

// Converts an IPv4-mapped IPv6 address to IPv4 address. Should only be called
// on IPv4-mapped IPv6 addresses.
// TODO:
// IPAddress ConvertIPv4MappedIPv6ToIPv4(const IPAddress& address) ;

// Compares an IP address to see if it falls within the specified IP block.
// Returns true if it does, false otherwise.
//
// The IP block is given by (|ip_prefix|, |prefix_length_in_bits|) -- any
// IP address whose |prefix_length_in_bits| most significant bits match
// |ip_prefix| will be matched.
//
// In cases when an IPv4 address is being compared to an IPv6 address prefix
// and vice versa, the IPv4 addresses will be converted to IPv4-mapped
// (IPv6) addresses.
// TODO:
// bool IPAddressMatchesPrefix(
//     const IPAddress& ip_address, const IPAddress& ip_prefix,
//     size_t prefix_length_in_bits) ;

// Parses an IP block specifier from CIDR notation to an
// (IP address, prefix length) pair. Returns true on success and fills
// |*ip_address| with the numeric value of the IP address and sets
// |*prefix_length_in_bits| with the length of the prefix.
//
// CIDR notation literals can use either IPv4 or IPv6 literals. Some examples:
//
//    10.10.3.1/20
//    a:b:c::/46
//    ::1/128
// TODO:
// bool ParseCIDRBlock(
//     const std::string& cidr_literal, IPAddress* ip_address,
//     size_t* prefix_length_in_bits) ;

// Parses a URL-safe IP literal (see RFC 3986, Sec 3.2.2) to its numeric value.
// Returns true on success, and fills |ip_address| with the numeric value.
// In other words, |hostname| must be an IPv4 literal, or an IPv6 literal
// surrounded by brackets as in [::1].
// TODO:
// V8_WARN_UNUSED_RESULT
// bool ParseURLHostnameToAddress(
//     const base::StringPiece& hostname, IPAddress* ip_address) ;

// Returns number of matching initial bits between the addresses |a1| and |a2|.
unsigned CommonPrefixLength(const IPAddress& a1, const IPAddress& a2) ;

// Computes the number of leading 1-bits in |mask|.
// TODO:
// unsigned MaskPrefixLength(const IPAddress& mask) ;

// Checks whether |address| starts with |prefix|. This provides similar
// functionality as IPAddressMatchesPrefix() but doesn't perform automatic IPv4
// to IPv4MappedIPv6 conversions and only checks against full bytes.
template <size_t N>
bool IPAddressStartsWith(
    const IPAddress& address, const std::uint8_t (&prefix)[N]) {
  if (address.size() < N)
    return false ;
  return std::equal(prefix, prefix + N, address.bytes().begin()) ;
}

#endif  // V8_VM_APPS_HTTP_SERVER_IP_ADDRESS_H_
