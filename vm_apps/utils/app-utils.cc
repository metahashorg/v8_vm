// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/utils/app-utils.h"

std::string ChangeFileExtension(
    const char* file_name, const char* new_extension) {
  // TODO: Implement
  return (std::string(file_name) + new_extension) ;
}

const char kWhitespaceASCII[] = {
  0x09,    // CHARACTER TABULATION
  0x0A,    // LINE FEED (LF)
  0x0B,    // LINE TABULATION
  0x0C,    // FORM FEED (FF)
  0x0D,    // CARRIAGE RETURN (CR)
  0x20,    // SPACE
  0
};

std::uint16_t StringToUint16(const char* str) {
  // TODO: Write more correct implementation
  // (see Chromium base/strings/string_number_conversions.*)
  return static_cast<std::uint16_t>(atol(str)) ;
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

TrimPositions TrimWhitespaceASCII(const std::string& input,
                                  TrimPositions positions,
                                  std::string* output) {
  return TrimStringT(input, kWhitespaceASCII, positions, output) ;
}
