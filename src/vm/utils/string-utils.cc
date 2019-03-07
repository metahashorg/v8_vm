// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/vm/utils/string-utils.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>

#include <algorithm>
#include <limits>
#include <vector>

#include "src/base/logging.h"
#include "src/base/macros.h"

namespace v8 {
namespace vm {
namespace internal {

namespace {

const char kWhitespaceASCII[] = {
  0x09,    // CHARACTER TABULATION
  0x0A,    // LINE FEED (LF)
  0x0B,    // LINE TABULATION
  0x0C,    // FORM FEED (FF)
  0x0D,    // CARRIAGE RETURN (CR)
  0x20,    // SPACE
  0
};

template<typename Str>
int CompareCaseInsensitiveASCIIT(const Str& a, const Str& b) {
  // Find the first characters that aren't equal and compare them.  If the end
  // of one of the strings is found before a nonequal character, the lengths
  // of the strings are compared.
  for (std::size_t i = 0; i < a.length() && i < b.length(); ++i) {
    typename Str::value_type lower_a = ToLowerASCII(a[i]) ;
    typename Str::value_type lower_b = ToLowerASCII(b[i]) ;
    if (lower_a < lower_b) {
      return -1 ;
    }

    if (lower_a > lower_b) {
      return 1 ;
    }
  }

  // End of one string hit before finding a different character. Expect the
  // common case to be "strings equal" at this point so check that first.
  if (a.length() == b.length()) {
    return 0 ;
  }

  if (a.length() < b.length()) {
    return -1 ;
  }

  return 1 ;
}

template<typename Str>
TrimPositions TrimStringT(const Str& input,
                          const typename Str::value_type* trim_chars,
                          TrimPositions positions,
                          Str* output) {
  const size_t last_char = input.length() - 1 ;
  const size_t first_good_char = (positions & TRIM_LEADING) ?
      input.find_first_not_of(trim_chars) : 0 ;
  const size_t last_good_char = (positions & TRIM_TRAILING) ?
      input.find_last_not_of(trim_chars) : last_char ;

  // When the string was all trimmed, report that we stripped off characters
  // from whichever position the caller was interested in. For empty input, we
  // stripped no characters, but we still need to clear |output|.
  if (input.empty() ||
      (first_good_char == Str::npos) || (last_good_char == Str::npos)) {
    bool input_was_empty = input.empty() ;  // in case output == &input
    output->clear() ;
    return input_was_empty ? TRIM_NONE : positions ;
  }

  // Trim.
  *output =
      input.substr(first_good_char, last_good_char - first_good_char + 1) ;

  // Return where we trimmed from.
  return static_cast<TrimPositions>(
      ((first_good_char == 0) ? TRIM_NONE : TRIM_LEADING) |
      ((last_good_char == last_char) ? TRIM_NONE : TRIM_TRAILING)) ;
}

template<typename StringType>
StringType ToLowerASCIIImpl(const StringType& str) {
  StringType ret;
  ret.reserve(str.size());
  for (size_t i = 0; i < str.size(); i++)
    ret.push_back(ToLowerASCII(str[i]));
  return ret;
}

template<typename StringType>
StringType ToUpperASCIIImpl(const StringType& str) {
  StringType ret;
  ret.reserve(str.size());
  for (size_t i = 0; i < str.size(); i++)
    ret.push_back(ToUpperASCII(str[i]));
  return ret;
}

}  // namespace

std::string ToLowerASCII(const std::string& str) {
  return ToLowerASCIIImpl<std::string>(str);
}

std::string ToUpperASCII(const std::string& str) {
  return ToUpperASCIIImpl<std::string>(str);
}

int CompareCaseInsensitiveASCII(const std::string& a, const std::string& b) {
  return CompareCaseInsensitiveASCIIT(a, b) ;
}

bool EqualsCaseInsensitiveASCII(const std::string& a, const std::string& b) {
  if (a.length() != b.length()) {
    return false ;
  }

  return CompareCaseInsensitiveASCIIT(a, b) == 0 ;
}

TrimPositions TrimWhitespaceASCII(
    const std::string& input, TrimPositions positions, std::string* output) {
  return TrimStringT(input, kWhitespaceASCII, positions, output) ;
}

// Implementation note: Normally this function will be called with a hardcoded
// constant for the lowercase_ascii parameter. Constructing a StringPiece from
// a C constant requires running strlen, so the result will be two passes
// through the buffers, one to file the length of lowercase_ascii, and one to
// compare each letter.
//
// This function could have taken a const char* to avoid this and only do one
// pass through the string. But the strlen is faster than the case-insensitive
// compares and lets us early-exit in the case that the strings are different
// lengths (will often be the case for non-matches). So whether one approach or
// the other will be faster depends on the case.
//
// The hardcoded strings are typically very short so it doesn't matter, and the
// string piece gives additional flexibility for the caller (doesn't have to be
// null terminated) so we choose the StringPiece route.
template<typename Str>
static inline bool DoLowerCaseEqualsASCII(
    const Str& str, const std::string& lowercase_ascii) {
  if (str.size() != lowercase_ascii.size())
    return false ;
  for (size_t i = 0; i < str.size(); i++) {
    if (ToLowerASCII(str[i]) != lowercase_ascii[i])
      return false ;
  }
  return true ;
}

bool LowerCaseEqualsASCII(
    const std::string& str, const std::string& lowercase_ascii) {
  return DoLowerCaseEqualsASCII<std::string>(str, lowercase_ascii) ;
}

bool LowerCaseEqualsASCII(
    const std::wstring& str, const std::string& lowercase_ascii) {
  return DoLowerCaseEqualsASCII<std::wstring>(str, lowercase_ascii);
}

template<typename Str>
bool StartsWithT(
    const Str& str, const Str& search_for, CompareCase case_sensitivity) {
  if (search_for.size() > str.size())
    return false ;

  Str source = str.substr(0, search_for.size()) ;

  switch (case_sensitivity) {
    case CompareCase::SENSITIVE:
      return source == search_for ;

    case CompareCase::INSENSITIVE_ASCII:
      return std::equal(
          search_for.begin(), search_for.end(),
          source.begin(),
          CaseInsensitiveCompareASCII<typename Str::value_type>()) ;

    default:
      UNREACHABLE() ;
      return false ;
  }
}

bool StartsWith(
    const std::string& str, const std::string& search_for,
    CompareCase case_sensitivity) {
  return StartsWithT<std::string>(str, search_for, case_sensitivity) ;
}

template <typename Str>
bool EndsWithT(
    const Str& str, const Str& search_for, CompareCase case_sensitivity) {
  if (search_for.size() > str.size())
    return false ;

  Str source = str.substr(str.size() - search_for.size(), search_for.size());

  switch (case_sensitivity) {
    case CompareCase::SENSITIVE:
      return source == search_for ;

    case CompareCase::INSENSITIVE_ASCII:
      return std::equal(
          source.begin(), source.end(),
          search_for.begin(),
          CaseInsensitiveCompareASCII<typename Str::value_type>()) ;

    default:
      UNREACHABLE() ;
      return false ;
  }
}

bool EndsWith(
    const std::string& str, const std::string& search_for,
    CompareCase case_sensitivity) {
  return EndsWithT<std::string>(str, search_for, case_sensitivity) ;
}

}  // namespace internal
}  // namespace vm
}  // namespace v8
