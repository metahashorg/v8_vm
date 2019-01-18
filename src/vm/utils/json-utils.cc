// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/vm/utils/json-utils.h"

namespace {

// Translation table to escape Latin1 characters.
// Table entries start at a multiple of 8 and are null-terminated.
const char* const JsonEscapeTable =
    "\\u0000\0 \\u0001\0 \\u0002\0 \\u0003\0 "
    "\\u0004\0 \\u0005\0 \\u0006\0 \\u0007\0 "
    "\\b\0     \\t\0     \\n\0     \\u000b\0 "
    "\\f\0     \\r\0     \\u000e\0 \\u000f\0 "
    "\\u0010\0 \\u0011\0 \\u0012\0 \\u0013\0 "
    "\\u0014\0 \\u0015\0 \\u0016\0 \\u0017\0 "
    "\\u0018\0 \\u0019\0 \\u001a\0 \\u001b\0 "
    "\\u001c\0 \\u001d\0 \\u001e\0 \\u001f\0 "
    " \0      !\0      \\\"\0     #\0      "
    "$\0      %\0      &\0      '\0      "
    "(\0      )\0      *\0      +\0      "
    ",\0      -\0      .\0      /\0      "
    "0\0      1\0      2\0      3\0      "
    "4\0      5\0      6\0      7\0      "
    "8\0      9\0      :\0      ;\0      "
    "<\0      =\0      >\0      ?\0      "
    "@\0      A\0      B\0      C\0      "
    "D\0      E\0      F\0      G\0      "
    "H\0      I\0      J\0      K\0      "
    "L\0      M\0      N\0      O\0      "
    "P\0      Q\0      R\0      S\0      "
    "T\0      U\0      V\0      W\0      "
    "X\0      Y\0      Z\0      [\0      "
    "\\\\\0     ]\0      ^\0      _\0      "
    "`\0      a\0      b\0      c\0      "
    "d\0      e\0      f\0      g\0      "
    "h\0      i\0      j\0      k\0      "
    "l\0      m\0      n\0      o\0      "
    "p\0      q\0      r\0      s\0      "
    "t\0      u\0      v\0      w\0      "
    "x\0      y\0      z\0      {\0      "
    "|\0      }\0      ~\0      \x7F\0      "
    "\x80\0      \x81\0      \x82\0      \x83\0      "
    "\x84\0      \x85\0      \x86\0      \x87\0      "
    "\x88\0      \x89\0      \x8A\0      \x8B\0      "
    "\x8C\0      \x8D\0      \x8E\0      \x8F\0      "
    "\x90\0      \x91\0      \x92\0      \x93\0      "
    "\x94\0      \x95\0      \x96\0      \x97\0      "
    "\x98\0      \x99\0      \x9A\0      \x9B\0      "
    "\x9C\0      \x9D\0      \x9E\0      \x9F\0      "
    "\xA0\0      \xA1\0      \xA2\0      \xA3\0      "
    "\xA4\0      \xA5\0      \xA6\0      \xA7\0      "
    "\xA8\0      \xA9\0      \xAA\0      \xAB\0      "
    "\xAC\0      \xAD\0      \xAE\0      \xAF\0      "
    "\xB0\0      \xB1\0      \xB2\0      \xB3\0      "
    "\xB4\0      \xB5\0      \xB6\0      \xB7\0      "
    "\xB8\0      \xB9\0      \xBA\0      \xBB\0      "
    "\xBC\0      \xBD\0      \xBE\0      \xBF\0      "
    "\xC0\0      \xC1\0      \xC2\0      \xC3\0      "
    "\xC4\0      \xC5\0      \xC6\0      \xC7\0      "
    "\xC8\0      \xC9\0      \xCA\0      \xCB\0      "
    "\xCC\0      \xCD\0      \xCE\0      \xCF\0      "
    "\xD0\0      \xD1\0      \xD2\0      \xD3\0      "
    "\xD4\0      \xD5\0      \xD6\0      \xD7\0      "
    "\xD8\0      \xD9\0      \xDA\0      \xDB\0      "
    "\xDC\0      \xDD\0      \xDE\0      \xDF\0      "
    "\xE0\0      \xE1\0      \xE2\0      \xE3\0      "
    "\xE4\0      \xE5\0      \xE6\0      \xE7\0      "
    "\xE8\0      \xE9\0      \xEA\0      \xEB\0      "
    "\xEC\0      \xED\0      \xEE\0      \xEF\0      "
    "\xF0\0      \xF1\0      \xF2\0      \xF3\0      "
    "\xF4\0      \xF5\0      \xF6\0      \xF7\0      "
    "\xF8\0      \xF9\0      \xFA\0      \xFB\0      "
    "\xFC\0      \xFD\0      \xFE\0      \xFF\0      " ;

static const int kJsonEscapeTableEntrySize = 8 ;

// Checks symbol for necessaty to be escape
bool DoNotEscape(char c) {
  return c >= '#' && c <= '~' && c != '\\' ;
}

}  // namespace

namespace v8 {
namespace vm {
namespace internal {

std::string EncodeJsonString(const std::string& str) {
  std::string result ;
  int begin = 0 ;
  for (int index = 0, length = (int)str.length(); index < length; ++index) {
    if (!DoNotEscape(str.at(index))) {
      result.append(str.c_str() + begin, index - begin) ;
      result += &JsonEscapeTable[str.at(index) * kJsonEscapeTableEntrySize] ;
      begin = index + 1 ;
    }
  }

  result += str.c_str() + begin ;
  return result ;
}

}  // namespace internal
}  // namespace vm
}  // namespace v8
