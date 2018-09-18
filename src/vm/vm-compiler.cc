// Copyright 2018 the AdSniper project authors. All rights reserved.
//
//

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
    Local<String> source_string = Utf8ToStr(isolate, file_content.start()) ;
    // TODO: Cut path, use the only a file name
    ScriptOrigin script_origin(
      Utf8ToStr(isolate, script_path), Local<v8::Integer>(),
      Local<v8::Integer>(), Local<v8::Boolean>(), Local<v8::Integer>(),
      Local<v8::Value>(), Local<v8::Boolean>(), Local<v8::Boolean>(),
      True(isolate) /* is_module */) ;
    ScriptCompiler::Source source(source_string, script_origin) ;
    ScriptCompiler::CompileOptions options = ScriptCompiler::kNoCompileOptions ;
    Local<Module> module = ScriptCompiler::CompileModule(
        isolate, &source, options).ToLocalChecked() ;
    ScriptCompiler::CachedData* cache =
        ScriptCompiler::CreateCodeCache(module->GetUnboundModuleScript()) ;
    i::WriteBytes(
        ChangeFileExtension(script_path, ".cmpl").c_str(),
        cache->data, cache->length, true) ;
  }

  isolate->Dispose() ;
}

Local<Module> LoadCompilation(
    Isolate* isolate, Local<Context> context, const char* compilation_path) {
  int file_size = 0 ;
  std::unique_ptr<i::byte> file_content(
      i::ReadBytes(compilation_path, &file_size, true)) ;
  if (file_size == 0) {
    return Local<Module>() ;
  }

#ifdef DEBUG
  // Use for a verbose deserialization
  TemporarilySetValue<bool> profile_deserialization(
      i::FLAG_profile_deserialization, true) ;
#endif  // DEBUG

  ScriptCompiler::CachedData* cache =
      new ScriptCompiler::CachedData(file_content.get(), file_size) ;
  cache->use_hash_for_check = false ;
  Local<String> source_string = Utf8ToStr(isolate, "") ;
  ScriptOrigin script_origin(
      Utf8ToStr(isolate, "memory"), Local<v8::Integer>(), Local<v8::Integer>(),
      Local<v8::Boolean>(), Local<v8::Integer>(), Local<v8::Value>(),
      Local<v8::Boolean>(), Local<v8::Boolean>(),
      True(isolate) /* is_module */) ;
  ScriptCompiler::Source source(source_string, script_origin, cache) ;
  ScriptCompiler::CompileOptions option = ScriptCompiler::kConsumeCodeCache ;

#ifdef DEBUG
  // For checking that we only load compilation.
  i::DisallowCompilation no_compile(reinterpret_cast<i::Isolate*>(isolate)) ;
#endif  // DEBUG

  Local<Module> module =
      ScriptCompiler::CompileModule(isolate, &source, option).ToLocalChecked() ;
  return module ;
}

}  // namespace internal
}  // namespace vm
}  // namespace v8
