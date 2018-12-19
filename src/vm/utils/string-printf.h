// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_UTILS_STRING_PRINTF_H_
#define V8_VM_UTILS_STRING_PRINTF_H_

#include <string>

#include "src/base/build_config.h"
#include "src/base/compiler-specific.h"

namespace v8 {
namespace vm {
namespace internal {

// Return a C++ string given printf-like input.
std::string StringPrintf(_Printf_format_string_ const char* format, ...)
    PRINTF_FORMAT(1, 2) V8_WARN_UNUSED_RESULT ;

// Return a C++ string given vprintf-like input.
std::string StringPrintV(const char* format, va_list ap)
    PRINTF_FORMAT(1, 0) V8_WARN_UNUSED_RESULT ;

// Store result into a supplied string and return it.
const std::string& SStringPrintf(
    std::string* dst, _Printf_format_string_ const char* format, ...)
    PRINTF_FORMAT(2, 3) ;

// Append result to a supplied string.
void StringAppendF(
    std::string* dst, _Printf_format_string_ const char* format, ...)
    PRINTF_FORMAT(2, 3) ;

// Lower-level routine that takes a va_list and appends to a specified
// string.  All other routines are just convenience wrappers around it.
void StringAppendV(std::string* dst, const char* format, va_list ap)
    PRINTF_FORMAT(2, 0) ;

}  // namespace internal
}  // namespace vm
}  // namespace v8

#endif  // V8_VM_UTILS_STRING_PRINTF_H_
