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

Error ScriptRunner::Run() {
  V8_LOG_FUNCTION_BODY() ;

  // Get script from cache
  Local<Script> script ;
  Error result = CompileScript(
      *context_, script_data_, script, script_cache_.get()) ;
  if (V8_ERROR_FAILED(result)) {
    V8_LOG_RETURN V8_ERROR_ADD_MSG(result, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  if (script_cache_->rejected) {
    V8_LOG_RETURN V8_ERROR_CREATE_WITH_MSG(
        errJSCacheRejected, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  if (script.IsEmpty()) {
    V8_LOG_RETURN V8_ERROR_CREATE_WITH_MSG(
        errJSUnknown, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  // Run script
  TryCatch try_catch(*context_) ;
  if (!script->Run(*context_).ToLocal(&result_)) {
    Error result = errJSUnknown ;
    V8_ERROR_CREATE_BY_TRY_CATCH(*context_, result, try_catch) ;
    V8_LOG_RETURN result ;
  }

  V8_LOG_INF("Result of command: %s", ValueToUtf8(*context_, result_).c_str()) ;
  V8_LOG_RETURN errOk ;
}

Error ScriptRunner::Create(
    const Data* data, const Data& script_data,
    std::unique_ptr<ScriptRunner>& runner, StartupData* snapshot_out) {
  DCHECK_EQ(Data::Type::JSScript, script_data.type) ;

  V8_LOG_FUNCTION_BODY() ;

  Error res = errOk ;

  // Create ScriptRunner and load a main script
  std::unique_ptr<ScriptRunner> result ;
  if (!data || data->type == Data::Type::None) {
    result.reset(new ScriptRunner(nullptr, snapshot_out)) ;
  } else {
    if (data->type == Data::Type::JSScript) {
      // Create ScriptRunner
      result.reset(new ScriptRunner(nullptr, snapshot_out)) ;

      // Compile a main script
      res = CompileScript(*result->context_, *data, result->main_script_) ;
      if (V8_ERROR_FAILED(res)) {
        V8_LOG_RETURN V8_ERROR_ADD_MSG(
            res, "Main script hasn't been compiled") ;
      }

      if (result->main_script_.IsEmpty()) {
        V8_LOG_RETURN V8_ERROR_CREATE_WITH_MSG(
            errJSUnknown, "Main script compiling've ended failure") ;
      }
    } else if (data->type == Data::Type::Compilation) {
      // Create ScriptRunner
      result.reset(new ScriptRunner(nullptr, snapshot_out)) ;

      // Load compilation
      res = LoadScriptCompilation(
          *result->context_, *data, result->main_script_) ;
      if (V8_ERROR_FAILED(res)) {
        V8_LOG_RETURN V8_ERROR_ADD_MSG(res, "Main script hasn't been loaded") ;
      }

      if (result->main_script_.IsEmpty()) {
        V8_LOG_RETURN V8_ERROR_CREATE_WITH_MSG(
            errJSUnknown, "Main script loading've ended failure") ;
      }
    } else if (data->type == Data::Type::Snapshot) {
      // Create ScriptRunner
      StartupData snapshot = { data->data, data->size } ;
      result.reset(new ScriptRunner(&snapshot, snapshot_out)) ;
    } else {
      V8_LOG_RETURN V8_ERROR_CREATE_WITH_MSG_SP(
          errInvalidArgument, "Arguments of \'%s\' are wrong", V8_FUNCTION) ;
    }
  }

  // We need to run a main script for using it
  if (data && (data->type == Data::Type::JSScript ||
      data->type == Data::Type::Compilation)) {
    TryCatch try_catch(*result->context_) ;
    Local<Value> run_result ;
    if (!result->main_script_->Run(*result->context_).ToLocal(&run_result)) {
      V8_ERROR_CREATE_BY_TRY_CATCH(*result->context_, res, try_catch) ;
      V8_LOG_RETURN res ;
    }
  }

  // Compile a command script and save a cache of it
  Local<Script> script ;
  res = CompileScript(*result->context_, script_data, script) ;
  if (V8_ERROR_FAILED(res)) {
    V8_ERROR_ADD_MSG(res, "Command script hasn't been compiled") ;
    V8_LOG_RETURN res ;
  }

  if (script.IsEmpty()) {
    V8_LOG_RETURN V8_ERROR_CREATE_WITH_MSG(
        errJSUnknown, "Command script compiling've ended failure") ;
  }

  result->script_cache_.reset(ScriptCompiler::CreateCodeCache(
      script->GetUnboundScript())) ;

  // Remember origin and source of script
  result->script_data_ = script_data ;
  result->script_data_.CopyData(script_data.data, script_data.size) ;
  std::swap(runner, result) ;
  V8_LOG_RETURN errOk ;
}

Error ScriptRunner::CreateByFiles(
    Data::Type file_type, const char* file_path, const char* script_path,
    std::unique_ptr<ScriptRunner>& runner, StartupData* snapshot_out) {
  // Load a script
  bool file_exists = false ;
  i::Vector<const char> script_source =
      i::ReadFile(script_path, &file_exists, true) ;
  if (!file_exists || script_source.size() == 0) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        (file_exists ? errFileEmpty : errFileNotExists),
        "Can't read the command script file - \'%s\'", script_path) ;
  }

  // Load a file content
  i::Vector<const char> main_script_source ;
  Data data(file_type, file_path) ;
  if (file_type == Data::Type::JSScript) {
    main_script_source = i::ReadFile(file_path, &file_exists, true) ;
    if (!file_exists || script_source.size() == 0) {
      return V8_ERROR_CREATE_WITH_MSG_SP(
          (file_exists ? errFileEmpty : errFileNotExists),
          "Can't read the main script file - \'%s\'", file_path) ;
    }

    data.data = main_script_source.start() ;
    data.size = static_cast<int>(strlen(data.data)) + 1 ;
  } else if (file_type == Data::Type::Compilation ||
             file_type == Data::Type::Snapshot) {
    data.data = reinterpret_cast<const char*>(
        i::ReadBytes(file_path, &data.size, true)) ;
    data.owner = true ;
    if (data.size == 0) {
      return V8_ERROR_CREATE_WITH_MSG_SP(
          errFileNotExists, "File doesn't exist or is empty - \'%s\'",
          file_path) ;
    }
  } else if (file_type != Data::Type::None) {
    return V8_ERROR_CREATE_WITH_MSG(
        errInvalidArgument, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  Data script_data(Data::Type::JSScript, script_path, script_source.start()) ;
  return Create(&data, script_data, runner, snapshot_out) ;
}

ScriptRunner::ScriptRunner(StartupData* snapshot, StartupData* snapshot_out)
  : context_(WorkContext::New(snapshot, snapshot_out)) {}

}  // namespace internal
}  // namespace vm
}  // namespace v8
