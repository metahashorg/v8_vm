// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_UTILS_JSON_UTILS_H_
#define V8_VM_UTILS_JSON_UTILS_H_

#include <iostream>
#include <string>
#include <vector>

#include "include/v8-vm.h"

namespace v8 {
namespace vm {
namespace internal {

// Json defines
#define JSON_ARRAY_OF_FIELD(name) \
  { "\"" #name "\":", "\"" #name "\": " }

#define JSON_FIELD(field, gap) \
  (("\"" + field + "\"") + ((int)gap ? ": " : ":"))

#define JSON_STRING(str) \
  ("\"" + EncodeJsonString(str) + "\"")

typedef std::vector<std::string> JsonGapArray ;

// Json special symbol list
constexpr const char* kJsonComma[] = { ",", ",\n" } ;
constexpr const char* kJsonEmptyArray[] = { "[]", "[]" } ;
constexpr const char* kJsonGap[] = { "", "  " } ;
constexpr const char* kJsonLeftBracket[] = { "{", "{\n" } ;
constexpr const char* kJsonLeftSquareBracket[] = { "[", "[\n" } ;
constexpr const char* kJsonNewLine[] = { "", "\n" } ;
constexpr const char* kJsonRightBracket[] = { "}", "}" } ;
constexpr const char* kJsonRightSquareBracket[] = { "]", "]" } ;

// Encodes a string for Json
std::string EncodeJsonString(const std::string& str) ;

// Class-wrapper of Json gaps if we use a formatted ouput
class JsonGap {
 public:
  JsonGap(JsonGapArray& gaps, FormattedJson formatted, int index)
    : gaps_(gaps), formatted_(formatted), index_(index) {
    if (gaps.size() == 0) {
      gaps.push_back("") ;
    }

    for (int i = (int)gaps.size(); i <= index; ++i) {
      gaps.push_back(kJsonGap[*this] + gaps[i - 1]) ;
    }
  }

  JsonGap(const JsonGap& parent)
    : JsonGap(parent.gaps_, parent.formatted_, parent.index_ + 1) {}

  int index() const { return index_ ; }

  operator const char*() const {
    return gaps_[index_].c_str() ;
  }

  operator int() const {
    return formatted_ == FormattedJson::True ? 1 : 0 ;
  }

 private:
  JsonGapArray& gaps_ ;
  FormattedJson formatted_ ;
  int index_ ;
};

// Writes JsonGap into std::ostream
static inline std::ostream& operator <<(std::ostream& os, const JsonGap& gap) {
  os << (const char*)gap ;
  return os ;
}

}  // namespace internal
}  // namespace vm
}  // namespace v8

#endif  // V8_VM_UTILS_JSON_UTILS_H_
