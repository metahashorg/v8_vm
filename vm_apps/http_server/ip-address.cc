// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/ip-address.h"

#include <algorithm>
#include <climits>

#include "vm_apps/third_party/url/gurl.h"
#include "vm_apps/third_party/url/url_canon_ip.h"
#include "vm_apps/utils/app-utils.h"

namespace {

// The prefix for IPv6 mapped IPv4 addresses.
// https://tools.ietf.org/html/rfc4291#section-2.5.5.2
const std::uint8_t kIPv4MappedPrefix[] =
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF} ;

// Note that this function assumes:
// * |ip_address| is at least |prefix_length_in_bits| (bits) long;
// * |ip_prefix| is at least |prefix_length_in_bits| (bits) long.
bool IPAddressPrefixCheck(const IPAddressBytes& ip_address,
                          const std::uint8_t* ip_prefix,
                          size_t prefix_length_in_bits) {
  // Compare all the bytes that fall entirely within the prefix.
  size_t num_entire_bytes_in_prefix = prefix_length_in_bits / 8 ;
  for (size_t i = 0; i < num_entire_bytes_in_prefix; ++i) {
    if (ip_address[i] != ip_prefix[i])
      return false ;
  }

  // In case the prefix was not a multiple of 8, there will be 1 byte
  // which is only partially masked.
  size_t remaining_bits = prefix_length_in_bits % 8 ;
  if (remaining_bits != 0) {
    std::uint8_t mask = 0xFF << (8 - remaining_bits) ;
    size_t i = num_entire_bytes_in_prefix ;
    if ((ip_address[i] & mask) != (ip_prefix[i] & mask))
      return false ;
  }
  return true ;
}

// Returns true if |ip_address| matches any of the reserved IPv4 ranges. This
// method operates on a blacklist of reserved IPv4 ranges. Some ranges are
// consolidated.
// Sources for info:
// www.iana.org/assignments/ipv4-address-space/ipv4-address-space.xhtml
// www.iana.org/assignments/iana-ipv4-special-registry/
// iana-ipv4-special-registry.xhtml
bool IsReservedIPv4(const IPAddressBytes& ip_address) {
  // Different IP versions have different range reservations.
  // TODO: DCHECK_EQ(IPAddress::kIPv4AddressSize, ip_address.size()) ;
  struct {
    const std::uint8_t address[4] ;
    size_t prefix_length_in_bits ;
  } static const kReservedIPv4Ranges[] = {
      {{0, 0, 0, 0}, 8},     {{10, 0, 0, 0}, 8},      {{100, 64, 0, 0}, 10},
      {{127, 0, 0, 0}, 8},   {{169, 254, 0, 0}, 16},  {{172, 16, 0, 0}, 12},
      {{192, 0, 2, 0}, 24},  {{192, 88, 99, 0}, 24},  {{192, 168, 0, 0}, 16},
      {{198, 18, 0, 0}, 15}, {{198, 51, 100, 0}, 24}, {{203, 0, 113, 0}, 24},
      {{224, 0, 0, 0}, 3}} ;

  for (const auto& range : kReservedIPv4Ranges) {
    if (IPAddressPrefixCheck(ip_address, range.address,
                             range.prefix_length_in_bits)) {
      return true ;
    }
  }

  return false ;
}

// Returns true if |ip_address| matches any of the reserved IPv6 ranges. This
// method operates on a whitelist of non-reserved IPv6 ranges. All IPv6
// addresses outside these ranges are reserved.
// Sources for info:
// www.iana.org/assignments/ipv6-address-space/ipv6-address-space.xhtml
bool IsReservedIPv6(const IPAddressBytes& ip_address) {
  // Different IP versions have different range reservations.
  // TODO: DCHECK_EQ(IPAddress::kIPv6AddressSize, ip_address.size()) ;
  struct {
    const std::uint8_t address_prefix[2] ;
    size_t prefix_length_in_bits ;
  } static const kPublicIPv6Ranges[] = {
      // 2000::/3  -- Global Unicast
      {{0x20, 0}, 3},
      // ff00::/8  -- Multicast
      {{0xff, 0}, 8},
  } ;

  for (const auto& range : kPublicIPv6Ranges) {
    if (IPAddressPrefixCheck(ip_address, range.address_prefix,
                             range.prefix_length_in_bits)) {
      return false ;
    }
  }

  return true ;
}

bool ParseIPLiteralToBytes(const std::string& ip_literal,
                           IPAddressBytes* bytes) {
  // |ip_literal| could be either an IPv4 or an IPv6 literal. If it contains
  // a colon however, it must be an IPv6 address.
  if (ip_literal.find(':') != std::string::npos) {
    // GURL expects IPv6 hostnames to be surrounded with brackets.
    std::string host_brackets = "[" ;
    // ip_literal.AppendToString(&host_brackets) ;
    host_brackets += ip_literal ;
    host_brackets.push_back(']') ;
    url::Component host_comp(0, static_cast<int>(host_brackets.length())) ;

    // Try parsing the hostname as an IPv6 literal.
    bytes->Resize(16) ;  // 128 bits.
    return url::IPv6AddressToNumber(host_brackets.data(), host_comp,
                                    bytes->data()) ;
  }

  // Otherwise the string is an IPv4 address.
  bytes->Resize(4) ;  // 32 bits.
  url::Component host_comp(0, static_cast<int>(ip_literal.length())) ;
  int num_components ;
  url::CanonHostInfo::Family family = url::IPv4AddressToNumber(
      ip_literal.data(), host_comp, bytes->data(), &num_components) ;
  return family == url::CanonHostInfo::IPV4 ;
}

}  // namespace

IPAddressBytes::IPAddressBytes() : size_(0) {}

IPAddressBytes::IPAddressBytes(const std::uint8_t* data, size_t data_len) {
  Assign(data, data_len) ;
}

IPAddressBytes::~IPAddressBytes() {}
IPAddressBytes::IPAddressBytes(IPAddressBytes const& other) = default ;

void IPAddressBytes::Assign(const std::uint8_t* data, size_t data_len) {
  size_ = data_len ;
  // TODO: CHECK_GE(16u, data_len) ;
  std::copy_n(data, data_len, bytes_.data()) ;
}

bool IPAddressBytes::operator<(const IPAddressBytes& other) const {
  if (size_ == other.size_)
    return std::lexicographical_compare(begin(), end(), other.begin(),
                                        other.end()) ;
  return size_ < other.size_ ;
}

bool IPAddressBytes::operator==(const IPAddressBytes& other) const {
  return size_ == other.size_ && std::equal(begin(), end(), other.begin()) ;
}

bool IPAddressBytes::operator!=(const IPAddressBytes& other) const {
  return !(*this == other) ;
}

IPAddress::IPAddress() {}

IPAddress::IPAddress(const IPAddress& other) = default ;

IPAddress::IPAddress(const IPAddressBytes& address) : ip_address_(address) {}

IPAddress::IPAddress(const std::uint8_t* address, size_t address_len)
    : ip_address_(address, address_len) {}

IPAddress::IPAddress(
    std::uint8_t b0, std::uint8_t b1, std::uint8_t b2, std::uint8_t b3) {
  ip_address_.push_back(b0) ;
  ip_address_.push_back(b1) ;
  ip_address_.push_back(b2) ;
  ip_address_.push_back(b3) ;
}

IPAddress::IPAddress(std::uint8_t b0,
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
                     std::uint8_t b15) {
  ip_address_.push_back(b0) ;
  ip_address_.push_back(b1) ;
  ip_address_.push_back(b2) ;
  ip_address_.push_back(b3) ;
  ip_address_.push_back(b4) ;
  ip_address_.push_back(b5) ;
  ip_address_.push_back(b6) ;
  ip_address_.push_back(b7) ;
  ip_address_.push_back(b8) ;
  ip_address_.push_back(b9) ;
  ip_address_.push_back(b10) ;
  ip_address_.push_back(b11) ;
  ip_address_.push_back(b12) ;
  ip_address_.push_back(b13) ;
  ip_address_.push_back(b14) ;
  ip_address_.push_back(b15) ;
}

IPAddress::~IPAddress() {}

bool IPAddress::IsIPv4() const {
  return ip_address_.size() == kIPv4AddressSize ;
}

bool IPAddress::IsIPv6() const {
  return ip_address_.size() == kIPv6AddressSize ;
}

bool IPAddress::IsValid() const {
  return IsIPv4() || IsIPv6();
}

bool IPAddress::IsReserved() const {
  if (IsIPv4()) {
    return IsReservedIPv4(ip_address_) ;
  } else if (IsIPv6()) {
    return IsReservedIPv6(ip_address_) ;
  }
  return false ;
}

bool IPAddress::IsZero() const {
  for (auto x : ip_address_) {
    if (x != 0)
      return false ;
  }

  return !empty() ;
}

bool IPAddress::IsIPv4MappedIPv6() const {
  return IsIPv6() && IPAddressStartsWith(*this, kIPv4MappedPrefix) ;
}

bool IPAddress::AssignFromIPLiteral(const std::string& ip_literal) {
  IPAddressBytes number ;

  // TODO(rch): change the contract so ip_address_ is cleared on failure,
  // to avoid needing this temporary at all.
  if (!ParseIPLiteralToBytes(ip_literal, &number))
    return false ;

  ip_address_ = number ;
  return true ;
}

std::vector<std::uint8_t> IPAddress::CopyBytesToVector() const {
  return std::vector<std::uint8_t>(ip_address_.begin(), ip_address_.end()) ;
}

// static
IPAddress IPAddress::IPv4Localhost() {
  static const std::uint8_t kLocalhostIPv4[] = {127, 0, 0, 1} ;
  return IPAddress(kLocalhostIPv4) ;
}

// static
IPAddress IPAddress::IPv6Localhost() {
  static const std::uint8_t kLocalhostIPv6[] = {0, 0, 0, 0, 0, 0, 0, 0,
                                                0, 0, 0, 0, 0, 0, 0, 1} ;
  return IPAddress(kLocalhostIPv6) ;
}

// static
IPAddress IPAddress::AllZeros(size_t num_zero_bytes) {
  // TODO: CHECK_LE(num_zero_bytes, 16u) ;
  IPAddress result ;
  for (size_t i = 0; i < num_zero_bytes; ++i)
    result.ip_address_.push_back(0u) ;
  return result ;
}

// static
IPAddress IPAddress::IPv4AllZeros() {
  return AllZeros(kIPv4AddressSize) ;
}

// static
IPAddress IPAddress::IPv6AllZeros() {
  return AllZeros(kIPv6AddressSize) ;
}

bool IPAddress::operator==(const IPAddress& that) const {
  return ip_address_ == that.ip_address_ ;
}

bool IPAddress::operator!=(const IPAddress& that) const {
  return ip_address_ != that.ip_address_ ;
}

bool IPAddress::operator<(const IPAddress& that) const {
  // Sort IPv4 before IPv6.
  if (ip_address_.size() != that.ip_address_.size()) {
    return ip_address_.size() < that.ip_address_.size() ;
  }

  return ip_address_ < that.ip_address_ ;
}

std::string IPAddress::ToString() const {
  std::string str ;
  url::StdStringCanonOutput output(&str) ;

  if (IsIPv4()) {
    url::AppendIPv4Address(ip_address_.data(), &output) ;
  } else if (IsIPv6()) {
    url::AppendIPv6Address(ip_address_.data(), &output) ;
  }

  output.Complete() ;
  return str ;
}

std::string IPAddressToStringWithPort(
    const IPAddress& address, std::uint16_t port) {
  std::string address_str = address.ToString() ;
  if (address_str.empty())
    return address_str ;

  if (address.IsIPv6()) {
    // Need to bracket IPv6 addresses since they contain colons.
    return StringPrintf("[%s]:%d", address_str.c_str(), port) ;
  }
  return StringPrintf("%s:%d", address_str.c_str(), port) ;
}

std::string IPAddressToPackedString(const IPAddress& address) {
  return std::string(reinterpret_cast<const char*>(address.bytes().data()),
                     address.size()) ;
}

// TODO:
// IPAddress ConvertIPv4ToIPv4MappedIPv6(const IPAddress& address) {
//   DCHECK(address.IsIPv4()) ;
//   // IPv4-mapped addresses are formed by:
//   // <80 bits of zeros>  + <16 bits of ones> + <32-bit IPv4 address>.
//   base::StackVector<std::uint8_t, 16> bytes ;
//   bytes->insert(bytes->end(), std::begin(kIPv4MappedPrefix),
//                 std::end(kIPv4MappedPrefix)) ;
//   bytes->insert(bytes->end(), address.bytes().begin(), address.bytes().end()) ;
//   return IPAddress(bytes->data(), bytes->size()) ;
// }

// TODO:
// IPAddress ConvertIPv4MappedIPv6ToIPv4(const IPAddress& address) {
//   DCHECK(address.IsIPv4MappedIPv6()) ;
//
//   base::StackVector<std::uint8_t, 16> bytes ;
//   bytes->insert(bytes->end(),
//                 address.bytes().begin() + arraysize(kIPv4MappedPrefix),
//                 address.bytes().end()) ;
//   return IPAddress(bytes->data(), bytes->size()) ;
// }

// TODO:
// bool IPAddressMatchesPrefix(const IPAddress& ip_address,
//                             const IPAddress& ip_prefix,
//                             size_t prefix_length_in_bits) {
//   // Both the input IP address and the prefix IP address should be either IPv4
//   // or IPv6.
//   DCHECK(ip_address.IsValid()) ;
//   DCHECK(ip_prefix.IsValid()) ;
//
//   DCHECK_LE(prefix_length_in_bits, ip_prefix.size() * 8) ;
//
//   // In case we have an IPv6 / IPv4 mismatch, convert the IPv4 addresses to
//   // IPv6 addresses in order to do the comparison.
//   if (ip_address.size() != ip_prefix.size()) {
//     if (ip_address.IsIPv4()) {
//       return IPAddressMatchesPrefix(ConvertIPv4ToIPv4MappedIPv6(ip_address),
//                                     ip_prefix, prefix_length_in_bits) ;
//     }
//     return IPAddressMatchesPrefix(ip_address,
//                                   ConvertIPv4ToIPv4MappedIPv6(ip_prefix),
//                                   96 + prefix_length_in_bits) ;
//   }
//
//   return IPAddressPrefixCheck(ip_address.bytes(), ip_prefix.bytes().data(),
//                               prefix_length_in_bits) ;
// }

// TODO:
// bool ParseCIDRBlock(const std::string& cidr_literal,
//                     IPAddress* ip_address,
//                     size_t* prefix_length_in_bits) {
//   // We expect CIDR notation to match one of these two templates:
//   //   <IPv4-literal> "/" <number of bits>
//   //   <IPv6-literal> "/" <number of bits>
//
//   std::vector<base::StringPiece> parts = base::SplitStringPiece(
//       cidr_literal, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL) ;
//   if (parts.size() != 2)
//     return false ;
//
//   // Parse the IP address.
//   if (!ip_address->AssignFromIPLiteral(parts[0]))
//     return false ;
//
//   // Parse the prefix length.
//   std::uint32_t number_of_bits ;
//   if (!ParseUint32(parts[1], &number_of_bits))
//     return false ;
//
//   // Make sure the prefix length is in a valid range.
//   if (number_of_bits > ip_address->size() * 8)
//     return false ;
//
//   *prefix_length_in_bits = number_of_bits ;
//   return true ;
// }

// TODO:
// bool ParseURLHostnameToAddress(const base::StringPiece& hostname,
//                                IPAddress* ip_address) {
//   if (hostname.size() >= 2 && hostname.front() == '[' &&
//       hostname.back() == ']') {
//     // Strip the square brackets that surround IPv6 literals.
//     auto ip_literal =
//         base::StringPiece(hostname).substr(1, hostname.size() - 2) ;
//     return ip_address->AssignFromIPLiteral(ip_literal) && ip_address->IsIPv6() ;
//   }
//
//   return ip_address->AssignFromIPLiteral(hostname) && ip_address->IsIPv4() ;
// }

unsigned CommonPrefixLength(const IPAddress& a1, const IPAddress& a2) {
  // TODO: DCHECK_EQ(a1.size(), a2.size()) ;
  for (size_t i = 0; i < a1.size(); ++i) {
    unsigned diff = a1.bytes()[i] ^ a2.bytes()[i] ;
    if (!diff)
      continue ;
    for (unsigned j = 0; j < CHAR_BIT; ++j) {
      if (diff & (1 << (CHAR_BIT - 1)))
        return static_cast<unsigned>(i * CHAR_BIT + j) ;
      diff <<= 1 ;
    }
    // TODO: NOTREACHED() ;
  }
  return static_cast<unsigned>(a1.size() * CHAR_BIT) ;
}

// TODO:
// unsigned MaskPrefixLength(const IPAddress& mask) {
//   base::StackVector<std::uint8_t, 16> all_ones ;
//   all_ones->resize(mask.size(), 0xFF) ;
//   return CommonPrefixLength(mask,
//                             IPAddress(all_ones->data(), all_ones->size())) ;
// }
