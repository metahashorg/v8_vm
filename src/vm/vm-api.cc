// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-vm.h"

#include <fstream>

#include "src/vm/dumper.h"
#include "src/vm/script-runner.h"
#include "src/vm/v8-handle.h"
#include "src/vm/vm-compiler.h"
#include "src/vm/vm-utils.h"

namespace vi = v8::vm::internal ;

namespace v8 {
namespace vm {

namespace {

enum class DumpType {
  Context = 1,
  Heap = 2,
  HeapGraph = 3,
};

void CreateDumpBySnapshotFromFile(
    DumpType type, const char* snapshot_path, v8::vm::FormattedJson formatted,
    const char* result_path) {
  vi::Data data(vi::Data::Type::Snapshot, snapshot_path) ;
  data.data = reinterpret_cast<const char*>(
    i::ReadBytes(snapshot_path, &data.size, true)) ;
  data.owner = true ;
  if (data.size == 0) {
    printf("ERROR: Snapshot is corrupted (\'%s\')\n", snapshot_path) ;
    return ;
  }

  std::fstream fs(result_path, std::fstream::out | std::fstream::binary) ;
  if (fs.fail()) {
    printf("ERROR: Can't open file \'%s\'\n", result_path) ;
    return ;
  }

  StartupData snapshot = { data.data, data.size } ;
  std::unique_ptr<vi::WorkContext> context(vi::WorkContext::New(&snapshot)) ;
  if (type == DumpType::Context) {
    CreateContextDump(*context, fs, formatted) ;
  } else if (type == DumpType::Heap) {
    CreateHeapDump(*context, fs) ;
  } else {
    CreateHeapGraphDump(*context, fs, formatted) ;
  }

  fs.close() ;

  printf(
      "INFO: Created a dump by the snapshot-file \'%s\' and "
      "saved result into \'%s\'\n",
      snapshot_path, result_path) ;
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

}  // namespace

void InitializeV8(const char* app_path) {
  V8HANDLE()->Initialize(app_path) ;
}

void DeinitializeV8() {
  V8HANDLE()->Deinitialize() ;
}

void CompileScriptFromFile(const char* script_path, const char* result_path) {
  vi::CompileScriptFromFile(script_path, result_path) ;
}

void CreateContextDumpBySnapshotFromFile(
    const char* snapshot_path, FormattedJson formatted,
    const char* result_path) {
  CreateDumpBySnapshotFromFile(
      DumpType::Context, snapshot_path, formatted, result_path) ;
}

void CreateHeapDumpBySnapshotFromFile(
    const char* snapshot_path, const char* result_path) {
  CreateDumpBySnapshotFromFile(
      DumpType::Heap, snapshot_path, FormattedJson::False, result_path) ;
}

void CreateHeapGraphDumpBySnapshotFromFile(
    const char* snapshot_path, FormattedJson formatted,
    const char* result_path) {
  CreateDumpBySnapshotFromFile(
      DumpType::HeapGraph, snapshot_path, formatted, result_path) ;
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
