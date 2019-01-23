// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-vm.h"

#include <fstream>

#include "src/utils.h"
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

Error RunScriptByFile(
    vi::Data::Type file_type, const char* file_path, const char* script_path,
    const char* snapshot_out_path /*= nullptr*/) {
  StartupData data = { nullptr, 0 }, *pdata = nullptr ;
  bool save_snapshot = (snapshot_out_path && strlen(snapshot_out_path)) ;
  if (save_snapshot) {
    pdata = &data ;
  }

  std::unique_ptr<vi::ScriptRunner> runner ;
  Error result = vi::ScriptRunner::CreateByFiles(
      file_type, file_path, script_path, runner, pdata) ;
  if (V8_ERROR_FAILED(result)) {
    printf("ERROR: Can't create ScriptRunner\n") ;
    return result ;
  }

  result = runner->Run() ;
  if (V8_ERROR_FAILED(result)) {
    printf("ERROR: Script run is failed\n") ;
    return result ;
  }

  // We've obtained a snapshot only after destruction of runner
  runner.reset() ;
  if (save_snapshot) {
    i::WriteChars(snapshot_out_path, data.data, data.raw_size, true) ;
    if (data.raw_size) {
      delete [] data.data ;
    }
  }

  return errOk ;
}

}  // namespace

std::size_t Error::message_count() const {
  return (messages_ ? messages_->size() : 0) + 1 ;
}

const Error::Message& Error::message(std::size_t index) const {
  std::size_t message_count = (messages_ ? messages_->size() : 0) ;

  if (error_message_position_ > message_count) {
    error_message_position_ = message_count ;
  }

  if (index == error_message_position_ || index > message_count) {
    if (!error_message_) {
      error_message_.reset(new Message()) ;
    }

    error_message_->message = description() ;
    error_message_->file = file() ;
    error_message_->line = line() ;
    return *error_message_ ;
  }

  return (*messages_)[index < error_message_position_ ? index : index - 1] ;
}

Error& Error::AddMessage(
    const std::string& msg, const char* file, std::uint32_t line,
    std::uint32_t back_offset, bool write_log) {
  std::size_t msg_count = message_count() ;
  // Check the back offset for we don't move fixed messages
  if (back_offset > (msg_count - fixed_message_count_)) {
    back_offset = static_cast<std::uint32_t>(msg_count - fixed_message_count_) ;
  }

  // Check a position of an error message
  if ((msg_count - back_offset) <= error_message_position_) {
    ++error_message_position_ ;
    --back_offset ;
  }

  // Create queue of messages
  if (!messages_) {
    messages_ = std::make_shared<std::deque<Message>>() ;
  }

  // Find a position for insertion
  auto pos = messages_->end() ;
  for (; back_offset > 0 && pos != messages_->begin(); --back_offset, --pos) ;

  // Insert message
  messages_->insert(pos, Message{msg, file, line}) ;

  // Write log
  if (write_log) {
    printf(
        "%s: %s (Error:%s(0x%08x) File:%s Line:%i)\n",
        V8_ERROR_FAILED(*this) ?
            "ERROR" : (code_ == errOk) ? "INFO" : "WARN", msg.c_str(),
        name(), code_, file, line) ;
  }

  return *this ;
}

void Error::FixCurrentMessageQueue() {
  fixed_message_count_ = message_count() ;
}

void InitializeV8(const char* app_path) {
  V8HANDLE()->Initialize(app_path) ;
}

void DeinitializeV8() {
  V8HANDLE()->Deinitialize() ;
}

Error CompileScript(
    const char* script, const char* script_origin,
    ScriptCompiler::CachedData& result) {
  vi::Data data ;
  Error res = vi::CompileScript(script, script_origin, data) ;
  if (V8_ERROR_FAILED(res)) {
    printf("ERROR: Can't compile the script\n") ;
    return res ;
  }

  // Clean a result
  if (result.data != nullptr &&
      result.buffer_policy == ScriptCompiler::CachedData::BufferOwned) {
    delete [] result.data ;
    result.data = nullptr ;
  }

  // Save a compilation into a result
  result.length = data.size ;
  result.buffer_policy = ScriptCompiler::CachedData::BufferOwned ;
  if (data.owner) {
    result.data = reinterpret_cast<const std::uint8_t*>(data.data) ;
    data.owner = false ;
  } else {
    result.data = new uint8_t[data.size] ;
    memcpy(const_cast<uint8_t*>(result.data), data.data, data.size) ;
  }

  return errOk ;
}

Error CompileScriptFromFile(const char* script_path, const char* result_path) {
  return vi::CompileScriptFromFile(script_path, result_path) ;
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

Error RunScript(
    const char* script, const char* script_origin /*= nullptr*/,
    StartupData* snapshot_out /*= nullptr*/) {
  printf("VERBS: v8::vm::RunScript().\n") ;

  vi::Data script_data(vi::Data::Type::JSScript, script_origin, script) ;
  std::unique_ptr<vi::ScriptRunner> runner ;
  Error result = vi::ScriptRunner::Create(
      nullptr, script_data, runner, snapshot_out) ;
  if (V8_ERROR_FAILED(result)) {
    printf("ERROR: Can't create ScriptRunner\n") ;
    return result ;
  }

  result = runner->Run() ;
  if (V8_ERROR_FAILED(result)) {
    printf("ERROR: Script run is failed\n") ;
    return result ;
  }

  // We've obtained a snapshot only after destruction of runner
  runner.reset() ;
  return result ;
}

Error RunScriptByJSScriptFromFile(
    const char* js_path, const char* script_path,
    const char* snapshot_out_path /*= nullptr*/) {
  return RunScriptByFile(
      vi::Data::Type::JSScript, js_path, script_path, snapshot_out_path) ;
}

Error RunScriptByCompilationFromFile(
    const char* compilation_path, const char* script_path,
    const char* snapshot_out_path /*= nullptr*/) {
  return RunScriptByFile(
      vi::Data::Type::Compilation, compilation_path, script_path,
      snapshot_out_path) ;
}

Error RunScriptBySnapshot(
    StartupData& snapshot, const char* script,
    const char* snapshot_origin /*= nullptr*/,
    const char* script_origin /*= nullptr*/,
    StartupData* snapshot_out /*= nullptr*/) {
  vi::Data snapshot_data(
      vi::Data::Type::Snapshot, snapshot_origin,
      snapshot.data, snapshot.raw_size) ;
  vi::Data script_data(vi::Data::Type::JSScript, script_origin, script) ;
  std::unique_ptr<vi::ScriptRunner> runner ;
  Error result = vi::ScriptRunner::Create(
      &snapshot_data, script_data, runner, snapshot_out) ;
  if (V8_ERROR_FAILED(result)) {
    printf("ERROR: Can't create ScriptRunner\n") ;
    return result ;
  }

  return runner->Run() ;
}

Error RunScriptBySnapshotFromFile(
    const char* snapshot_path, const char* script_path,
    const char* snapshot_out_path /*= nullptr*/) {
  return RunScriptByFile(
      vi::Data::Type::Snapshot, snapshot_path, script_path,
      snapshot_out_path) ;
}

}  // namespace vm
}  // namespace v8
