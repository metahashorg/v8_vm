// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/utils/app-utils.h"

#include <cstdio>
#include <cwchar>
#include <locale>

#include "src/base/macros.h"

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
  std::locale locale ;
  for (std::size_t i = 0; i < a.length() && i < b.length(); ++i) {
    typename Str::value_type lower_a = std::tolower(a[i], locale) ;
    typename Str::value_type lower_b = std::tolower(b[i], locale) ;
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

}  // namespace

std::string ChangeFileExtension(
    const char* file_name, const char* new_extension) {
  // TODO: Implement
  return (std::string(file_name) + new_extension) ;
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

TrimPositions TrimWhitespaceASCII(const std::string& input,
                                  TrimPositions positions,
                                  std::string* output) {
  return TrimStringT(input, kWhitespaceASCII, positions, output) ;
}
