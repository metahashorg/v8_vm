// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_APP_UTILS_H_
#define V8_VM_APPS_APP_UTILS_H_

#include <string>

// The arraysize(arr) macro returns the count of elements in an array arr. The
// expression is a compile-time constant, and therefore can be used in defining
// new arrays, for example. If you use arraysize on a pointer by mistake, you
// will get a compile-time error. For the technical details, refer to
// http://blogs.msdn.com/b/the1/archive/2004/05/07/128242.aspx.

// This template function declaration is used in defining arraysize.
// Note that the function doesn't need an implementation, as we only
// use its type.
template <typename T, size_t N> char (&ArraySizeHelper(T (&array)[N]))[N] ;
#define arraysize(array) (sizeof(ArraySizeHelper(array)))


// Trims any whitespace from either end of the input string.
//
// The std::string versions return where whitespace was found.
// NOTE: Safe to use the same variable for both input and output.
enum TrimPositions {
  TRIM_NONE     = 0,
  TRIM_LEADING  = 1 << 0,
  TRIM_TRAILING = 1 << 1,
  TRIM_ALL      = TRIM_LEADING | TRIM_TRAILING,
};

TrimPositions TrimWhitespaceASCII(const std::string& input,
                                  TrimPositions positions,
                                  std::string* output) ;

#endif  // V8_VM_APPS_COMMAND_LINE_H_
