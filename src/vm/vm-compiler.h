// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_VM_COMPILER_H_
#define V8_VM_VM_COMPILER_H_

#include "include/v8-vm.h"
#include "src/vm/utils/vm-utils.h"

namespace v8 {
namespace vm {
namespace internal {

Error CompileModule(
    Isolate* isolate, const Data& module_data, Local<Module>& module,
    ScriptCompiler::CachedData* cache = nullptr) ;

Error CompileScript(
    Local<Context> context, const Data& script_data, Local<Script>& script,
    ScriptCompiler::CachedData* cache = nullptr) ;

Error CompileModuleFromFile(const char* module_path, const char* result_path) ;

Error CompileScript(
    const char* script, const char* script_origin, Data& result) ;

Error CompileScriptFromFile(const char* script_path, const char* result_path) ;

Error LoadModuleCompilation(
    Isolate* isolate, const Data& compilation_data, Local<Module>& module) ;

Error LoadScriptCompilation(
    Local<Context> context, const Data& compilation_data,
    Local<Script>& script) ;

}  // namespace internal
}  // namespace vm
}  // namespace v8

#endif  // V8_VM_VM_COMPILER_H_
