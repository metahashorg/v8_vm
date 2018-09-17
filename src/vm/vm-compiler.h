// Copyright 2018 the AdSniper project authors. All rights reserved.
//
//

#ifndef V8_VM_VM_COMPILER_H_
#define V8_VM_VM_COMPILER_H_

#include "include/v8.h"

namespace v8 {
namespace vm {
namespace internal {

void CompileScript(const char* script_path) ;

Local<Module> LoadCompilation(
    Isolate* isolate, Local<Context> context, const char* compilation_path) ;

}  // namespace internal
}  // namespace vm
}  // namespace v8

#endif  // V8_VM_VM_COMPILER_H_
