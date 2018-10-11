// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-vm.h"

#include "src/vm/script-runner.h"
#include "src/vm/v8-handle.h"
#include "src/vm/vm-compiler.h"

namespace vi = v8::vm::internal ;

namespace v8 {
namespace vm {

void InitializeV8(const char* app_path) {
  vi::V8Handle::instance_.Pointer()->Initialize(app_path) ;
}

void DeinitializeV8() {
  vi::V8Handle::instance_.Pointer()->Deinitialize() ;
}

void CompileScript(const char* script_path) {
  vi::CompileScript(script_path) ;
}

void RunScriptByCompilation(
    const char* compilation_path, const char* script_path) {
  vi::RunScriptByCompilation(compilation_path, script_path) ;
}

}  // namespace vm
}  // namespace v8
