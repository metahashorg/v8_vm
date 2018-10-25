// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_VM_COMPILER_H_
#define V8_VM_VM_COMPILER_H_

#include "include/v8.h"
#include "src/vm/vm-utils.h"

namespace v8 {
namespace vm {
namespace internal {

Local<Module> CompileModule(
    Isolate* isolate, const Data& module_data,
    ScriptCompiler::CachedData* cache = nullptr) ;

Local<Script> CompileScript(
    Local<Context> context, const Data& script_data,
    ScriptCompiler::CachedData* cache = nullptr) ;

void CompileModuleFromFile(const char* module_path, const char* result_path) ;

void CompileScriptFromFile(const char* script_path, const char* result_path) ;

Local<Module> LoadModuleCompilation(
    Isolate* isolate, const Data& compilation_data) ;

Local<Script> LoadScriptCompilation(
    Local<Context> context, const Data& compilation_data) ;

}  // namespace internal
}  // namespace vm
}  // namespace v8

#endif  // V8_VM_VM_COMPILER_H_
