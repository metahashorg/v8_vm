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
  V8HANDLE()->Initialize(app_path) ;
}

void DeinitializeV8() {
  V8HANDLE()->Deinitialize() ;
}

void CompileScript(const char* script_path) {
  vi::CompileScriptFromFile(script_path) ;
}

void RunScriptByCompilation(
    const char* compilation_path, const char* script_path,
    const char* out_snapshot_path /*= nullptr*/) {
  StartupData data = { nullptr, 0 }, *pdata = nullptr ;
  if (out_snapshot_path && strlen(out_snapshot_path)) {
    pdata = &data ;
  }

  std::unique_ptr<vi::ScriptRunner> runner(
      vi::ScriptRunner::CreateByCompilation(
          compilation_path, script_path, pdata)) ;
  if (!runner) {
    printf("ERROR: Can't create ScriptRunner\n") ;
    return ;
  }

  runner->Run() ;

  // We've obtained a snapshot only after destruction of runner
  runner.reset() ;
  if (out_snapshot_path && strlen(out_snapshot_path)) {
    i::WriteChars(out_snapshot_path, data.data, data.raw_size, true) ;
    if (data.raw_size) {
      delete [] data.data ;
    }
  }
}

void RunScriptBySnapshot(
    const char* snapshot_path, const char* script_path,
    const char* out_snapshot_path /*= nullptr*/) {
  StartupData data = { nullptr, 0 }, *pdata = nullptr ;
  if (out_snapshot_path && strlen(out_snapshot_path)) {
    pdata = &data ;
  }

  std::unique_ptr<vi::ScriptRunner> runner(
      vi::ScriptRunner::CreateBySnapshot(snapshot_path, script_path, pdata)) ;
  if (!runner) {
    printf("ERROR: Can't create ScriptRunner\n") ;
    return ;
  }

  runner->Run() ;

  // We've obtained a snapshot only after destruction of runner
  runner.reset() ;
  if (out_snapshot_path && strlen(out_snapshot_path)) {
    i::WriteChars(out_snapshot_path, data.data, data.raw_size, true) ;
    if (data.raw_size) {
      delete [] data.data ;
    }
  }
}

}  // namespace vm
}  // namespace v8
