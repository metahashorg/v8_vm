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

void CompileScriptFromFile(const char* script_path, const char* result_path) {
  vi::CompileScriptFromFile(script_path, result_path) ;
}

void RunScriptByFile(
    vi::Data::Type file_type, const char* file_path, const char* script_path,
    const char* out_snapshot_path /*= nullptr*/) {
  StartupData data = { nullptr, 0 }, *pdata = nullptr ;
  bool save_snapshot = (out_snapshot_path && strlen(out_snapshot_path)) ;
  if (save_snapshot) {
    pdata = &data ;
  }

  std::unique_ptr<vi::ScriptRunner> runner(vi::ScriptRunner::CreateByFiles(
      file_type, file_path, script_path, pdata)) ;
  if (!runner) {
    printf("ERROR: Can't create ScriptRunner\n") ;
    return ;
  }

  runner->Run() ;

  // We've obtained a snapshot only after destruction of runner
  runner.reset() ;
  if (save_snapshot) {
    i::WriteChars(out_snapshot_path, data.data, data.raw_size, true) ;
    if (data.raw_size) {
      delete [] data.data ;
    }
  }
}

void RunScriptByJSScriptFromFile(
    const char* js_path, const char* script_path,
    const char* out_snapshot_path /*= nullptr*/) {
  RunScriptByFile(
      vi::Data::Type::JSScript, js_path, script_path, out_snapshot_path) ;
}

void RunScriptByCompilationFromFile(
    const char* compilation_path, const char* script_path,
    const char* out_snapshot_path /*= nullptr*/) {
  RunScriptByFile(
      vi::Data::Type::Compilation, compilation_path, script_path,
      out_snapshot_path) ;
}

void RunScriptBySnapshotFromFile(
    const char* snapshot_path, const char* script_path,
    const char* out_snapshot_path /*= nullptr*/) {
  RunScriptByFile(
      vi::Data::Type::Snapshot, snapshot_path, script_path,
      out_snapshot_path) ;
}

}  // namespace vm
}  // namespace v8
