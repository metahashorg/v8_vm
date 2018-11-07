// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/vm/script-runner.h"

#include "src/utils.h"
#include "src/vm/v8-handle.h"
#include "src/vm/vm-compiler.h"

namespace v8 {
namespace vm {
namespace internal {

ScriptRunner::~ScriptRunner() {
  result_.Clear() ;
  script_cache_.reset() ;
  main_script_.Clear() ;
  context_.reset() ;
}

void ScriptRunner::Run() {
  // Get script from cache
  Local<Script> script = CompileScript(
      *context_, script_data_, script_cache_.get()) ;
  if (script.IsEmpty()){
    printf("ERROR: Command script hasn't been compiled\n") ;
    return ;
  }

  // Run script
  result_ = script->Run(*context_).ToLocalChecked() ;

  // TODO: Temporary output
  printf("INFO: Result of command: %s\n",
         ValueToUtf8(*context_, result_).c_str()) ;
}

ScriptRunner* ScriptRunner::Create(
    const Data* data, const Data& script_data,
    StartupData* snapshot_out /*= nullptr*/) {
  DCHECK_EQ(Data::Type::JSScript, script_data.type) ;

  // Create ScriptRunner and load a main script
  std::unique_ptr<ScriptRunner> result ;
  if (!data || data->type == Data::Type::None) {
    result.reset(new ScriptRunner()) ;
  } else {
    if (data->type == Data::Type::JSScript) {
      // Create ScriptRunner
      result.reset(new ScriptRunner(nullptr, snapshot_out)) ;

      // Compile a main script
      result->main_script_ = CompileScript(*result->context_, *data) ;
      if (result->main_script_.IsEmpty()) {
        printf("ERROR: Main script compiling've ended failure\n") ;
        return nullptr ;
      }

      // We need to run a main script for using it
      result->main_script_->Run(*result->context_).ToLocalChecked() ;
    } else if (data->type == Data::Type::Compilation) {
      // Create ScriptRunner
      result.reset(new ScriptRunner(nullptr, snapshot_out)) ;

      // Load compilation
      result->main_script_ = LoadScriptCompilation(*result->context_, *data) ;
      if (result->main_script_.IsEmpty()) {
        printf("ERROR: Main script loading've ended failure\n") ;
        return nullptr ;
      }

      // We need to run a main script for using it
      result->main_script_->Run(*result->context_).ToLocalChecked() ;
    } else if (data->type == Data::Type::Snapshot) {
      // Create ScriptRunner
      StartupData snapshot = { data->data, data->size } ;
      result.reset(new ScriptRunner(&snapshot, snapshot_out)) ;
    } else {
      printf("ERROR: Arguments of \'%s\' are wrong.\n", __FUNCTION__) ;
      return nullptr ;
    }
  }

  // Compile a command script and save a cache of it
  Local<Script> script = CompileScript(*result->context_, script_data) ;
  result->script_cache_.reset(ScriptCompiler::CreateCodeCache(
      script->GetUnboundScript())) ;
  if (script.IsEmpty()) {
    printf("ERROR: Main script compiling've ended failure "
           "(Script origin: %s)\n", script_data.origin.c_str()) ;
    return nullptr ;
  }

  // Remember origin and source of script
  result->script_data_ = script_data ;
  result->script_data_.CopyData(script_data.data, script_data.size) ;
  return result.release() ;
}

ScriptRunner* ScriptRunner::CreateByFiles(
    Data::Type file_type, const char* file_path, const char* script_path,
    StartupData* snapshot_out /*= nullptr*/) {
  // Load a script
  bool file_exists = false ;
  i::Vector<const char> script_source =
      i::ReadFile(script_path, &file_exists, true) ;
  if (!file_exists || script_source.size() == 0) {
    printf("ERROR: Script file doesn't exist or is empty (%s)\n", script_path) ;
    return nullptr ;
  }

  // Load a file content
  i::Vector<const char> main_script_source ;
  Data data(file_type, file_path) ;
  if (file_type == Data::Type::JSScript) {
    main_script_source = i::ReadFile(file_path, &file_exists, true) ;
    if (!file_exists || script_source.size() == 0) {
      printf("ERROR: Main script file doesn't exist or is empty (%s)\n",
             script_path) ;
      return nullptr ;
    }

    data.data = main_script_source.start() ;
    data.size = static_cast<int>(strlen(data.data)) + 1 ;
  } else if (file_type == Data::Type::Compilation ||
             file_type == Data::Type::Snapshot) {
    data.data = reinterpret_cast<const char*>(
        i::ReadBytes(file_path, &data.size, true)) ;
    data.owner = true ;
    if (data.size == 0) {
      printf("ERROR: File doesn't exist or is empty (%s)\n", file_path) ;
      return nullptr ;
    }
  } else if (file_type != Data::Type::None) {
    printf("ERROR: Arguments of \'%s\' are wrong.\n", __FUNCTION__) ;
    return nullptr ;
  }

  Data script_data(Data::Type::JSScript, script_path, script_source.start()) ;
  return Create(&data, script_data, snapshot_out) ;
}

ScriptRunner::ScriptRunner(
    StartupData* snapshot /*= nullptr*/,
    StartupData* snapshot_out /*= nullptr*/)
  : context_(WorkContext::New(snapshot, snapshot_out)) {}

}  // namespace internal
}  // namespace vm
}  // namespace v8
