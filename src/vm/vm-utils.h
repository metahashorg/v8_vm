// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_VM_UTILS_H_
#define V8_VM_VM_UTILS_H_

#include "src/handles.h"
#include "src/handles-inl.h"

namespace v8 {
namespace vm {
namespace internal {

// Converts a utf8-string into a internal V8 string
static inline Local<String> Utf8ToStr(Isolate* isolate, const char* str) {
  return String::NewFromUtf8(isolate, str, NewStringType::kNormal)
             .ToLocalChecked() ;
}

// Temporarily sets a value into a variable
// NOTE: Need to think of multithreading. Now here is a potential problem.
// E.g {var:true}->{thrd1 var:false,cache:true}->{thrd2 var:false,cache:false}->
//         {thrd1 var:true}->{thrd2 var:false}
template<typename T>
class TemporarilySetValue {
 public:
  explicit TemporarilySetValue(T& variable, T value)
      : variable_(variable), cache_(value) {
    std::swap(variable_, cache_) ;
  }

  ~TemporarilySetValue() {
    std::swap(variable_, cache_) ;
  }

 private:
  TemporarilySetValue() = delete ;

  T& variable_ ;
  T cache_ ;
};

// HandleScope can't be created by 'new()' but we need something for classes
class InitializedHandleScope {
 public:
  explicit InitializedHandleScope(Isolate* isolate)
      : handle_scope_(isolate) {}

 private:
  HandleScope handle_scope_ ;
};

}  // namespace internal
}  // namespace vm
}  // namespace v8

#endif  // V8_VM_VM_UTILS_H_
