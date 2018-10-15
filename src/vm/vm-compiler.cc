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
    Isolate* isolate, const char* resource_name, const char* source,
    v8::ScriptCompiler::CachedData* cache /*= nullptr*/) {
  // We need to create a copy of cache
  // because ScriptCompiler::Source will delete it
  ScriptCompiler::CachedData* local_cache = nullptr ;
  if (cache) {
    local_cache = new ScriptCompiler::CachedData(
        cache->data, cache->length,
        ScriptCompiler::CachedData::BufferNotOwned) ;
    local_cache->use_hash_for_check = cache->use_hash_for_check ;
  }

  Local<String> source_string = Utf8ToStr(isolate, source) ;
  ScriptOrigin script_origin(
      Utf8ToStr(isolate, resource_name), Local<v8::Integer>(),
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

void CompileScript(const char* script_path) {
  bool file_exists = false ;
  i::Vector<const char> file_content =
      i::ReadFile(script_path, &file_exists, true) ;
  if (!file_exists || file_content.size() == 0) {
    return ;
  }

  // Need for a full compilation
  // TODO: Look at other flags
  TemporarilySetValue<bool> lazy(i::FLAG_lazy, false) ;
  TemporarilySetValue<bool> log_code(i::FLAG_log_code, true) ;

  Isolate* isolate =
      Isolate::New(V8Handle::instance_.Pointer()->create_params()) ;

  {
    // TODO: Use TryCatch and output errors
    Isolate::Scope iscope(isolate) ;
    HandleScope scope(isolate) ;
    Local<Context> context = Context::New(isolate) ;
    Context::Scope cscope(context) ;

    Local<Module> module =
        CompileModule(isolate, script_path, file_content.start()) ;
    ScriptCompiler::CachedData* cache =
        ScriptCompiler::CreateCodeCache(module->GetUnboundModuleScript()) ;
    i::WriteBytes(
        ChangeFileExtension(script_path, ".cmpl").c_str(),
        cache->data, cache->length, true) ;
  }

  isolate->Dispose() ;
}

Local<Module> LoadCompilation(Isolate* isolate, const char* compilation_path) {
  int file_size = 0 ;
  i::byte* file_content(i::ReadBytes(compilation_path, &file_size, true)) ;
  if (file_size == 0) {
    printf("ERROR: File of compilation is empty (%s)\n", compilation_path) ;
    return Local<Module>() ;
  }

#ifdef DEBUG
  // Use for a verbose deserialization
  TemporarilySetValue<bool> profile_deserialization(
      i::FLAG_profile_deserialization, true) ;
#endif  // DEBUG

  std::unique_ptr<ScriptCompiler::CachedData> cache(
      new ScriptCompiler::CachedData(file_content, file_size,
                                     ScriptCompiler::CachedData::BufferOwned)) ;
  cache->use_hash_for_check = false ;
  Local<Module> module = CompileModule(isolate, "memory", "", cache.get()) ;

  // We can't use a compilation of a script source because it's fake
  if (cache->rejected) {
    printf("ERROR: The compilation is corrupted\n") ;
    module.Clear() ;
  }

  return module ;
}

}  // namespace internal
}  // namespace vm
}  // namespace v8
