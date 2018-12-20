// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-vm.h"

#include <fstream>

#include "src/vm/dumper.h"
#include "src/vm/script-runner.h"
#include "src/vm/utils/string-printf.h"
#include "src/vm/utils/vm-utils.h"
#include "src/vm/v8-handle.h"
#include "src/vm/vm-compiler.h"

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
    const char* snapshot_out_path /*= nullptr*/) {
  StartupData data = { nullptr, 0 }, *pdata = nullptr ;
  bool save_snapshot = (snapshot_out_path && strlen(snapshot_out_path)) ;
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
    i::WriteChars(snapshot_out_path, data.data, data.raw_size, true) ;
    if (data.raw_size) {
      delete [] data.data ;
    }
  }
}

}  // namespace

std::string ErrorToString(Error error) {
  const char* msg = "" ;
  switch(error) {
    // TODO: Add other error codes
    case errJsonInvalidEscape:
      msg = "Json: Invalid escape sequence." ;
      break ;
    case errJsonSyntaxError:
      msg = "Json: Syntax error." ;
      break ;
    case errJsonUnexpectedToken:
      msg = "Json: Unexpected token." ;
      break ;
    case errJsonTrailingComma:
      msg = "Json: Trailing comma not allowed." ;
      break ;
    case errJsonTooMuchNesting:
      msg = "Json: Too much nesting." ;
      break ;
    case errJsonUnexpectedDataAfterRoot:
      msg = "Json: Unexpected data after root element." ;
      break ;
    case errJsonUnsupportedEncoding:
      msg = "Json: Unsupported encoding. JSON must be UTF-8." ;
      break ;
    case errJsonUnquotedDictionaryKey:
      msg = "Json: Dictionary keys must be quoted." ;
      break ;
    case errJsonInappropriateType:
      msg = "Json: Field has an inappropriate type." ;
      break ;
    default:
      msg = "Error occurred." ;
      break ;
  }

  return vi::StringPrintf("%s Error code: 0x%08x.", msg, error) ;
}

void InitializeV8(const char* app_path) {
  V8HANDLE()->Initialize(app_path) ;
}

void DeinitializeV8() {
  V8HANDLE()->Deinitialize() ;
}

void CompileScript(
    const char* script, const char* script_origin,
    ScriptCompiler::CachedData& result) {
  vi::Data data ;
  vi::CompileScript(script, script_origin, data) ;
  if (data.type != vi::Data::Type::Compilation) {
    printf("ERROR: Can't compile the script\n") ;
    return ;
  }

  if (result.data != nullptr &&
      result.buffer_policy == ScriptCompiler::CachedData::BufferOwned) {
    delete [] result.data ;
    result.data = nullptr ;
  }

  result.length = data.size ;
  result.buffer_policy = ScriptCompiler::CachedData::BufferOwned ;
  if (data.owner) {
    result.data = reinterpret_cast<const std::uint8_t*>(data.data) ;
    data.owner = false ;
  } else {
    result.data = new uint8_t[data.size] ;
    memcpy(const_cast<uint8_t*>(result.data), data.data, data.size) ;
  }
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

void RunScript(
    const char* script, const char* script_origin /*= nullptr*/,
    StartupData* snapshot_out /*= nullptr*/) {
  vi::Data script_data(vi::Data::Type::JSScript, script_origin, script) ;
  std::unique_ptr<vi::ScriptRunner> runner(
      vi::ScriptRunner::Create(nullptr, script_data, snapshot_out)) ;
  if (!runner) {
    printf("ERROR: Can't create ScriptRunner\n") ;
    return ;
  }

  runner->Run() ;
}

void RunScriptByJSScriptFromFile(
    const char* js_path, const char* script_path,
    const char* snapshot_out_path /*= nullptr*/) {
  RunScriptByFile(
      vi::Data::Type::JSScript, js_path, script_path, snapshot_out_path) ;
}

void RunScriptByCompilationFromFile(
    const char* compilation_path, const char* script_path,
    const char* snapshot_out_path /*= nullptr*/) {
  RunScriptByFile(
      vi::Data::Type::Compilation, compilation_path, script_path,
      snapshot_out_path) ;
}

void RunScriptBySnapshot(
    StartupData& snapshot, const char* script,
    const char* snapshot_origin /*= nullptr*/,
    const char* script_origin /*= nullptr*/,
    StartupData* snapshot_out /*= nullptr*/) {
  vi::Data snapshot_data(
      vi::Data::Type::Snapshot, snapshot_origin,
      snapshot.data, snapshot.raw_size) ;
  vi::Data script_data(vi::Data::Type::JSScript, script_origin, script) ;
  std::unique_ptr<vi::ScriptRunner> runner(
      vi::ScriptRunner::Create(&snapshot_data, script_data, snapshot_out)) ;
  if (!runner) {
    printf("ERROR: Can't create ScriptRunner\n") ;
    return ;
  }

  runner->Run() ;
}

void RunScriptBySnapshotFromFile(
    const char* snapshot_path, const char* script_path,
    const char* snapshot_out_path /*= nullptr*/) {
  RunScriptByFile(
      vi::Data::Type::Snapshot, snapshot_path, script_path,
      snapshot_out_path) ;
}

}  // namespace vm
}  // namespace v8
