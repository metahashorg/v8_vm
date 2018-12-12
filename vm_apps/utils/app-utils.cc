// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/utils/app-utils.h"

#include <stdarg.h>

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

// Simple scoper that saves the current value of errno, resets it to 0, and on
// destruction puts the old value back.
class ScopedClearErrno {
 public:
  ScopedClearErrno() : old_errno_(errno) {
    errno = 0 ;
  }
  ~ScopedClearErrno() {
    if (errno == 0) {
      errno = old_errno_ ;
    }
  }

 private:
  const int old_errno_ ;

  DISALLOW_COPY_AND_ASSIGN(ScopedClearErrno) ;
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

// Overloaded wrappers around vsnprintf and vswprintf. The buf_size parameter
// is the size of the buffer. These return the number of characters in the
// formatted string excluding the NUL terminator. If the buffer is not
// large enough to accommodate the formatted string without truncation, they
// return the number of characters that would be in the fully-formatted string
// (vsnprintf, and vswprintf on Windows), or -1 (vswprintf on POSIX platforms).
inline int vsnprintfT(char* buffer,
                      size_t size,
                      const char* format,
                      va_list arguments) {
#if defined(V8_OS_WIN)
  int length = vsnprintf_s(buffer, size, size - 1, format, arguments) ;
  if (length < 0) {
    return _vscprintf(format, arguments) ;
  }

  return length ;
#else
  return ::vsnprintf(buffer, size, format, arguments) ;
#endif  // V8_OS_WIN
}

// TODO: Uncomment to use wchar_t
// inline int vsnprintfT(wchar_t* buffer,
//                       size_t size,
//                       const wchar_t* format,
//                       va_list arguments) {
// #if defined(V8_OS_WIN)
//   int length = _vsnwprintf_s(buffer, size, size - 1, format, arguments) ;
//   if (length < 0) {
//     return _vscwprintf(format, arguments) ;
//   }
//
//   return length ;
// #else
//   return ::vswprintf(buffer, size, format, arguments);
// #endif  // V8_OS_WIN
// }

// Templatized backend for StringPrintF/StringAppendF. This does not finalize
// the va_list, the caller is expected to do that.
template <class StringType>
static void StringAppendVT(StringType* dst,
                           const typename StringType::value_type* format,
                           va_list ap) {
  // First try with a small fixed size buffer.
  // This buffer size should be kept in sync with StringUtilTest.GrowBoundary
  // and StringUtilTest.StringPrintfBounds.
  typename StringType::value_type stack_buf[1024] ;

  va_list ap_copy ;
  va_copy(ap_copy, ap) ;

#if !defined(V8_OS_WIN)
  ScopedClearErrno clear_errno ;
#endif
  int result = vsnprintfT(stack_buf, arraysize(stack_buf), format, ap_copy) ;
  va_end(ap_copy) ;

  if (result >= 0 && result < static_cast<int>(arraysize(stack_buf))) {
    // It fit.
    dst->append(stack_buf, result) ;
    return ;
  }

  // Repeatedly increase buffer size until it fits.
  int mem_length = arraysize(stack_buf) ;
  while (true) {
    if (result < 0) {
#if defined(V8_OS_WIN)
      // On Windows, vsnprintfT always returns the number of characters in a
      // fully-formatted string, so if we reach this point, something else is
      // wrong and no amount of buffer-doubling is going to fix it.
      return ;
#else
      if (errno != 0 && errno != EOVERFLOW) {
        return ;
      }

      // Try doubling the buffer size.
      mem_length *= 2 ;
#endif
    } else {
      // We need exactly "result + 1" characters.
      mem_length = result + 1 ;
    }

    if (mem_length > 32 * 1024 * 1024) {
      // That should be plenty, don't try anything larger.  This protects
      // against huge allocations when using vsnprintfT implementations that
      // return -1 for reasons other than overflow without setting errno.
      printf("WARN: Unable to printf the requested string due to size.") ;
      return ;
    }

    std::vector<typename StringType::value_type> mem_buf(mem_length) ;

    // NOTE: You can only use a va_list once.  Since we're in a while loop, we
    // need to make a new copy each time so we don't use up the original.
    va_copy(ap_copy, ap) ;
    result = vsnprintfT(&mem_buf[0], mem_length, format, ap_copy) ;
    va_end(ap_copy) ;

    if ((result >= 0) && (result < mem_length)) {
      // It fit.
      dst->append(&mem_buf[0], result) ;
      return ;
    }
  }
}

void StringAppendV(std::string* dst, const char* format, va_list ap) {
  StringAppendVT(dst, format, ap) ;
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

void StringAppendF(std::string* dst, const char* format, ...) {
  va_list ap ;
  va_start(ap, format) ;
  StringAppendV(dst, format, ap) ;
  va_end(ap) ;
}

std::string StringPrintf(const char* format, ...) {
  va_list ap ;
  va_start(ap, format) ;
  std::string result ;
  StringAppendV(&result, format, ap) ;
  va_end(ap) ;
  return result ;
}

std::uint16_t StringToUint16(const char* str) {
  // TODO: Write more correct implementation
  // (see Chromium base/strings/string_number_conversions.*)
  return static_cast<std::uint16_t>(atol(str)) ;
}

std::int32_t StringToInt32(const char* str) {
  // TODO: Write more correct implementation
  // (see Chromium base/strings/string_number_conversions.*)
  return static_cast<std::int32_t>(atol(str)) ;
}

TrimPositions TrimWhitespaceASCII(const std::string& input,
                                  TrimPositions positions,
                                  std::string* output) {
  return TrimStringT(input, kWhitespaceASCII, positions, output) ;
}
