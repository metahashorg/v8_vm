// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines utility functions for working with strings.

#ifndef V8_VM_UTILS_STRING_UTILS_H_
#define V8_VM_UTILS_STRING_UTILS_H_

#include <ctype.h>
#include <stdarg.h>   // va_list
#include <stddef.h>
#include <stdint.h>

#include <initializer_list>
#include <string>
#include <vector>

#include "include/v8.h"
#include "src/base/compiler-specific.h"
#include "src/base/build_config.h"

namespace v8 {
namespace vm {
namespace internal {

// ASCII-specific tolower.  The standard library's tolower is locale sensitive,
// so we don't want to use it here.
inline char ToLowerASCII(char c) {
  return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c ;
}

// ASCII-specific toupper.  The standard library's toupper is locale sensitive,
// so we don't want to use it here.
inline char ToUpperASCII(char c) {
  return (c >= 'a' && c <= 'z') ? (c + ('A' - 'a')) : c ;
}

// Converts the given string to it's ASCII-lowercase equivalent.
V8_EXPORT std::string ToLowerASCII(const std::string& str) ;

// Converts the given string to it's ASCII-uppercase equivalent.
V8_EXPORT std::string ToUpperASCII(const std::string& str) ;

// Functor for case-insensitive ASCII comparisons for STL algorithms like
// std::search.
//
// Note that a full Unicode version of this functor is not possible to write
// because case mappings might change the number of characters, depend on
// context (combining accents), and require handling UTF-16. If you need
// proper Unicode support, use base::i18n::ToLower/FoldCase and then just
// use a normal operator== on the result.
template<typename Char> struct CaseInsensitiveCompareASCII {
 public:
  bool operator()(Char x, Char y) const {
    return ToLowerASCII(x) == ToLowerASCII(y) ;
  }
};

// Like strcasecmp for case-insensitive ASCII characters only. Returns:
//   -1  (a < b)
//    0  (a == b)
//    1  (a > b)
// (unlike strcasecmp which can return values greater or less than 1/-1). For
// full Unicode support, use base::i18n::ToLower or base::i18h::FoldCase
// and then just call the normal string operators on the result.
V8_EXPORT int CompareCaseInsensitiveASCII(
    const std::string& a, const std::string& b) ;

// Equality for ASCII case-insensitive comparisons. For full Unicode support,
// use base::i18n::ToLower or base::i18h::FoldCase and then compare with either
// == or !=.
V8_EXPORT bool EqualsCaseInsensitiveASCII(
    const std::string& a, const std::string& b) ;

enum TrimPositions {
  TRIM_NONE     = 0,
  TRIM_LEADING  = 1 << 0,
  TRIM_TRAILING = 1 << 1,
  TRIM_ALL      = TRIM_LEADING | TRIM_TRAILING,
};

// Trims any whitespace from either end of the input string.
//
// The StringPiece versions return a substring referencing the input buffer.
// The ASCII versions look only for ASCII whitespace.
//
// The std::string versions return where whitespace was found.
// NOTE: Safe to use the same variable for both input and output.
V8_EXPORT TrimPositions TrimWhitespaceASCII(
    const std::string& input, TrimPositions positions, std::string* output) ;

// Compare the lower-case form of the given string against the given
// previously-lower-cased ASCII string (typically a constant).
V8_EXPORT bool LowerCaseEqualsASCII(
    const std::string& str, const std::string& lowercase_ascii) ;

// Indicates case sensitivity of comparisons. Only ASCII case insensitivity
// is supported. Full Unicode case-insensitive conversions would need to go in
// base/i18n so it can use ICU.
//
// If you need to do Unicode-aware case-insensitive StartsWith/EndsWith, it's
// best to call base::i18n::ToLower() or base::i18n::FoldCase() (see
// base/i18n/case_conversion.h for usage advice) on the arguments, and then use
// the results to a case-sensitive comparison.
enum class CompareCase {
  SENSITIVE,
  INSENSITIVE_ASCII,
};

V8_EXPORT bool StartsWith(
    const std::string& str, const std::string& search_for,
    CompareCase case_sensitivity) ;
V8_EXPORT bool EndsWith(
    const std::string& str, const std::string& search_for,
    CompareCase case_sensitivity);

}  // namespace internal
}  // namespace vm
}  // namespace v8

#endif  // V8_VM_UTILS_STRING_UTILS_H_
