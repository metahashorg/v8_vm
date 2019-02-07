// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/vm/vm-compiler.h"

#include "src/assert-scope.h"
#include "src/globals.h"
#include "src/utils.h"
#include "src/vm/v8-handle.h"
#include "src/vm/work-context.h"

namespace v8 {
namespace vm {
namespace internal {

Error CompileModule(
    Isolate* isolate, const Data& module_data, Local<Module>& module,
    ScriptCompiler::CachedData* cache /*= nullptr*/) {
  DCHECK_EQ(Data::Type::JSScript, module_data.type) ;

  // We need to create a copy of cache
  // because ScriptCompiler::Source will delete it
  ScriptCompiler::CachedData* local_cache = nullptr ;
  if (cache) {
    local_cache = new ScriptCompiler::CachedData(
        cache->data, cache->length,
        ScriptCompiler::CachedData::BufferNotOwned) ;
    local_cache->use_hash_for_check = cache->use_hash_for_check ;
  }

  Local<String> source_string = Utf8ToStr(isolate, module_data.data) ;
  ScriptOrigin script_origin(
      Utf8ToStr(isolate, module_data.origin.c_str()), Local<v8::Integer>(),
      Local<v8::Integer>(), Local<v8::Boolean>(), Local<v8::Integer>(),
      Local<v8::Value>(), Local<v8::Boolean>(), Local<v8::Boolean>(),
      True(isolate) /* is_module */) ;
  ScriptCompiler::Source script_source(
      source_string, script_origin, local_cache) ;
  ScriptCompiler::CompileOptions option =
      local_cache ? ScriptCompiler::kConsumeCodeCache :
                    ScriptCompiler::kNoCompileOptions ;

  TryCatch try_catch(isolate) ;
  if (!ScriptCompiler::CompileModule(
          isolate, &script_source, option).ToLocal(&module)) {
    Error result = errJSUnknown ;
    if (try_catch.HasCaught()) {
      result = errJSException ;
      // TODO: We need v8::Context
      // V8_ERROR_ADD_MSG_BY_TRY_CATCH(context, result, try_catch) ;
      printf("ERROR: Exception occurred during comilation. (Message: %s)\n",
             ValueToUtf8(isolate, try_catch.Exception()).c_str()) ;
    } else {
      printf("ERROR: Unknown error occurred during comilation.") ;
    }

    return result ;
  }

  if (cache) {
    cache->rejected = local_cache->rejected ;
  }

  return errOk ;
}

Error CompileScript(
    Local<Context> context, const Data& script_data, Local<Script>& script,
    ScriptCompiler::CachedData* cache /*= nullptr*/) {
  DCHECK_EQ(Data::Type::JSScript, script_data.type) ;

  // We need to create a copy of cache
  // because ScriptCompiler::Source will delete it
  ScriptCompiler::CachedData* local_cache = nullptr ;
  if (cache) {
    local_cache = new ScriptCompiler::CachedData(
        cache->data, cache->length,
        ScriptCompiler::CachedData::BufferNotOwned) ;
    local_cache->use_hash_for_check = cache->use_hash_for_check ;
  }

  Isolate* isolate = context->GetIsolate() ;
  Local<String> source_string = Utf8ToStr(isolate, script_data.data) ;
  ScriptOrigin script_origin(Utf8ToStr(isolate, script_data.origin.c_str())) ;
  ScriptCompiler::Source script_source(
      source_string, script_origin, local_cache) ;
  ScriptCompiler::CompileOptions option =
      local_cache ? ScriptCompiler::kConsumeCodeCache :
                    ScriptCompiler::kNoCompileOptions ;

  TryCatch try_catch(context->GetIsolate()) ;
  if (!ScriptCompiler::Compile(
          context, &script_source, option).ToLocal(&script)) {
    Error result = errJSUnknown ;
    V8_ERROR_CREATE_BY_TRY_CATCH(context, result, try_catch) ;
    return result ;
  }

  if (cache) {
    cache->rejected = local_cache->rejected ;
  }

  return errOk ;
}

Error CompileModuleFromFile(const char* module_path, const char* result_path) {
  bool file_exists = false ;
  i::Vector<const char> file_content =
      i::ReadFile(module_path, &file_exists, true) ;
  if (!file_exists || file_content.size() == 0) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        (file_exists ? errFileEmpty : errFileNotExists),
        "Can't read the module script file - \'%s\'", module_path) ;
  }

  std::unique_ptr<WorkContext> context(WorkContext::New()) ;
  Data module_data(Data::Type::JSScript, module_path, file_content.start()) ;
  Local<Module> module ;
  Error res = CompileModule(context->isolate(), module_data, module) ;
  V8_ERROR_RETURN_IF_FAILED(res) ;

  if (module.IsEmpty()) {
    return V8_ERROR_CREATE_WITH_MSG(
        errUnknown, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  ScriptCompiler::CachedData* cache =
      ScriptCompiler::CreateCodeCache(module->GetUnboundModuleScript()) ;
  i::WriteBytes(result_path, cache->data, cache->length, true) ;
  V8_LOG_INF(
      "Compiled the file \'%s\' and saved result into \'%s\'\n",
      module_path, result_path) ;
  return errOk ;
}

Error CompileScript(
    const char* script, const char* script_origin, Data& result) {
  V8_LOG_FUNCTION_BODY() ;

  std::unique_ptr<WorkContext> context(WorkContext::New()) ;
  Data script_data(Data::Type::JSScript, script_origin, script) ;
  Local<Script> script_obj ;
  Error res = CompileScript(*context, script_data, script_obj) ;
  V8_ERROR_RETURN_IF_FAILED(res) ;

  if (script_obj.IsEmpty()) {
    return V8_ERROR_CREATE_WITH_MSG(
        errUnknown, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  ScriptCompiler::CachedData* cache =
      ScriptCompiler::CreateCodeCache(script_obj->GetUnboundScript()) ;
  result.type = Data::Type::Compilation ;
  result.origin = script_origin ? script_origin : "" ;
  result.CopyData(cache->data, cache->length) ;
  return errOk ;
}

Error CompileScriptFromFile(const char* script_path, const char* result_path) {
  bool file_exists = false ;
  i::Vector<const char> file_content =
      i::ReadFile(script_path, &file_exists, true) ;
  if (!file_exists || file_content.size() == 0) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        (file_exists ? errFileEmpty : errFileNotExists),
        "Can't read the script file - \'%s\'", script_path) ;
  }

  Data result ;
  Error res = CompileScript(file_content.start(), script_path, result) ;
  V8_ERROR_RETURN_IF_FAILED(res)

  i::WriteBytes(result_path, reinterpret_cast<const std::uint8_t*>(result.data),
                result.size, true) ;
  V8_LOG_MSG(
      "Compiled the file \'%s\' and saved result into \'%s\'\n",
      script_path, result_path) ;
  return errOk ;
}

Error LoadModuleCompilation(
    Isolate* isolate, const Data& compilation_data, Local<Module>& module) {
  DCHECK_EQ(Data::Type::Compilation, compilation_data.type) ;

  std::unique_ptr<ScriptCompiler::CachedData> cache(
    new ScriptCompiler::CachedData(
          reinterpret_cast<const uint8_t*>(compilation_data.data),
          compilation_data.size,
          ScriptCompiler::CachedData::BufferNotOwned)) ;
  cache->use_hash_for_check = false ;
  Data module_data(Data::Type::JSScript, compilation_data.origin.c_str(), "") ;
  Error result = CompileModule(isolate, module_data, module, cache.get()) ;
  V8_ERROR_RETURN_IF_FAILED(result) ;

  // We can't use a compilation of a script source because it's fake
  if (cache->rejected) {
    module.Clear() ;
    return V8_ERROR_CREATE_WITH_MSG(
        errJSCacheRejected, "The module compilation is corrupted") ;
  }

  return errOk ;
}

Error LoadScriptCompilation(
    Local<Context> context, const Data& compilation_data,
    Local<Script>& script) {
  DCHECK_EQ(Data::Type::Compilation, compilation_data.type) ;

  std::unique_ptr<ScriptCompiler::CachedData> cache(
      new ScriptCompiler::CachedData(
          reinterpret_cast<const uint8_t*>(compilation_data.data),
          compilation_data.size,
          ScriptCompiler::CachedData::BufferNotOwned)) ;
  cache->use_hash_for_check = false ;
  Data script_data(Data::Type::JSScript, compilation_data.origin.c_str(), "") ;
  Error result = CompileScript(context, script_data, script, cache.get()) ;
  V8_ERROR_RETURN_IF_FAILED(result) ;

  // We can't use a compilation of a script source because it's fake
  if (cache->rejected) {
    script.Clear() ;
    return V8_ERROR_CREATE_WITH_MSG(
        errJSCacheRejected, "The script compilation is corrupted") ;
  }

  return errOk ;
}

}  // namespace internal
}  // namespace vm
}  // namespace v8
