// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/vm/vm-compiler.h"

#include "src/assert-scope.h"
#include "src/flags.h"
#include "src/globals.h"
#include "src/utils.h"
#include "src/vm/v8-handle.h"
#include "src/vm/vm-utils.h"

namespace v8 {
namespace vm {
namespace internal {

std::string ChangeFileExtension(
    const char* file_name, const char* new_extension) {
  // TODO: Implement
  return (std::string(file_name) + new_extension) ;
}

Local<Module> CompileModule(
    Isolate* isolate, const Data& module_data,
    v8::ScriptCompiler::CachedData* cache /*= nullptr*/) {
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
  Local<Module> module =
      ScriptCompiler::CompileModule(isolate, &script_source, option)
          .ToLocalChecked() ;
  if (cache) {
    cache->rejected = local_cache->rejected ;
  }

  return module ;
}

Local<Script> CompileScript(
    Local<Context> context, const Data& script_data,
    v8::ScriptCompiler::CachedData* cache /*= nullptr*/) {
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
  Local<Script> script =
      ScriptCompiler::Compile(context, &script_source, option)
          .ToLocalChecked() ;
  if (cache) {
    cache->rejected = local_cache->rejected ;
  }

  return script ;
}

void CompileModuleFromFile(const char* module_path) {
  bool file_exists = false ;
  i::Vector<const char> file_content =
      i::ReadFile(module_path, &file_exists, true) ;
  if (!file_exists || file_content.size() == 0) {
    printf("ERROR: File of a module source is empty (%s)\n", module_path) ;
    return ;
  }

  // Need for a full compilation
  // TODO: Look at other flags
  TemporarilySetValue<bool> lazy(i::FLAG_lazy, false) ;
  TemporarilySetValue<bool> log_code(i::FLAG_log_code, true) ;

  Isolate* isolate = Isolate::New(V8HANDLE()->create_params()) ;

  {
    // TODO: Use TryCatch and output errors
    Isolate::Scope iscope(isolate) ;
    HandleScope scope(isolate) ;
    Local<Context> context = Context::New(isolate) ;
    Context::Scope cscope(context) ;
    Data module_data(Data::Type::JSScript, module_path, file_content.start()) ;
    Local<Module> module = CompileModule(isolate, module_data) ;
    ScriptCompiler::CachedData* cache =
        ScriptCompiler::CreateCodeCache(module->GetUnboundModuleScript()) ;
    i::WriteBytes(
        ChangeFileExtension(module_path, ".cmpl").c_str(),
        cache->data, cache->length, true) ;
  }

  isolate->Dispose() ;
}

void CompileScriptFromFile(const char* script_path) {
  bool file_exists = false ;
  i::Vector<const char> file_content =
      i::ReadFile(script_path, &file_exists, true) ;
  if (!file_exists || file_content.size() == 0) {
    printf("ERROR: File of a script source is empty (%s)\n", script_path) ;
    return ;
  }

  // Need for a full compilation
  // TODO: Look at other flags
  TemporarilySetValue<bool> lazy(i::FLAG_lazy, false) ;
  TemporarilySetValue<bool> log_code(i::FLAG_log_code, true) ;

  Isolate* isolate = Isolate::New(V8HANDLE()->create_params()) ;

  {
    // TODO: Use TryCatch and output errors
    Isolate::Scope iscope(isolate) ;
    HandleScope scope(isolate) ;
    Local<Context> context = Context::New(isolate) ;
    Context::Scope cscope(context) ;
    Data script_data(Data::Type::JSScript, script_path, file_content.start()) ;
    Local<Script> script = CompileScript(context, script_data) ;
    ScriptCompiler::CachedData* cache =
        ScriptCompiler::CreateCodeCache(script->GetUnboundScript()) ;
    i::WriteBytes(
        ChangeFileExtension(script_path, ".cmpl").c_str(),
        cache->data, cache->length, true) ;
  }

  isolate->Dispose() ;
}

Local<Module> LoadModuleCompilation(
    Isolate* isolate, const Data& compilation_data) {
  DCHECK_EQ(Data::Type::Compilation, compilation_data.type) ;

#ifdef DEBUG
  // Use for a verbose deserialization
  TemporarilySetValue<bool> profile_deserialization(
      i::FLAG_profile_deserialization, true) ;
#endif  // DEBUG

  std::unique_ptr<ScriptCompiler::CachedData> cache(
    new ScriptCompiler::CachedData(
          reinterpret_cast<const uint8_t*>(compilation_data.data),
          compilation_data.size,
          ScriptCompiler::CachedData::BufferNotOwned)) ;
  cache->use_hash_for_check = false ;
  Data module_data(Data::Type::JSScript, compilation_data.origin.c_str(), "") ;
  Local<Module> module = CompileModule(isolate, module_data, cache.get()) ;

  // We can't use a compilation of a script source because it's fake
  if (cache->rejected) {
    printf("ERROR: The module compilation is corrupted\n") ;
    module.Clear() ;
  }

  return module ;
}

Local<Script> LoadScriptCompilation(
    Local<Context> context, const Data& compilation_data) {
  DCHECK_EQ(Data::Type::Compilation, compilation_data.type) ;

#ifdef DEBUG
  // Use for a verbose deserialization
  TemporarilySetValue<bool> profile_deserialization(
      i::FLAG_profile_deserialization, true) ;
#endif  // DEBUG

  std::unique_ptr<ScriptCompiler::CachedData> cache(
      new ScriptCompiler::CachedData(
          reinterpret_cast<const uint8_t*>(compilation_data.data),
          compilation_data.size,
          ScriptCompiler::CachedData::BufferNotOwned)) ;
  cache->use_hash_for_check = false ;
  Data script_data(Data::Type::JSScript, compilation_data.origin.c_str(), "") ;
  Local<Script> script = CompileScript(context, script_data, cache.get()) ;

  // We can't use a compilation of a script source because it's fake
  if (cache->rejected) {
    printf("ERROR: The script compilation is corrupted\n") ;
    script.Clear() ;
  }

  return script ;
}

}  // namespace internal
}  // namespace vm
}  // namespace v8
