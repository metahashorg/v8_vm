// Copyright 2018 the AdSniper project authors. All rights reserved.
//
//

#ifndef V8_VM_VM_UTILS_H_
#define V8_VM_VM_UTILS_H_

namespace v8 {
namespace vm {
namespace internal {

static inline Local<String> Utf8ToStr(Isolate* isolate, const char* str) {
  return String::NewFromUtf8(isolate, str, NewStringType::kNormal)
             .ToLocalChecked() ;
}

}  // namespace internal
}  // namespace vm
}  // namespace v8

#endif  // V8_VM_VM_UTILS_H_
