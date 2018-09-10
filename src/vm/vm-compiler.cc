// Copyright 2018 the AdSniper project authors. All rights reserved.
//
//

#include "src/vm/vm-compiler.h"

#include "src/utils.h"
#include "src/vm/v8-handle.h"

namespace v8 {
namespace vm {
namespace internal {

Local<String> Ut8ToStr(Isolate* isolate, const char* str) {
  return String::NewFromUtf8(isolate, str, NewStringType::kNormal)
             .ToLocalChecked() ;
}

std::string ChangeFileExtension(
    const char* file_name, const char* new_extension) {
  // TODO: Implement
  return (std::string(file_name) + new_extension) ;
}

void CompileScript(const char* script_path) {
  bool file_exists = false ;
  i::Vector<const char> file_content =
      i::ReadFile(script_path, &file_exists, true) ;
  if (!file_exists) {
    return ;
  }

  Isolate* isolate =
      Isolate::New(V8Handle::instance_.Pointer()->create_params()) ;
  {
    // TODO: Use TryCatch and output errors
    // TODO: Try to use i::FLAG_*
    Isolate::Scope iscope(isolate) ;
    HandleScope scope(isolate) ;
    Local<Context> context = Context::New(isolate) ;
    Context::Scope cscope(context) ;
    Local<String> source_string = Ut8ToStr(isolate, file_content.start()) ;
    ScriptOrigin script_origin(Ut8ToStr(isolate, script_path)) ;
    ScriptCompiler::Source source(source_string, script_origin) ;
    ScriptCompiler::CompileOptions options = ScriptCompiler::kNoCompileOptions ;
    Local<Script> script =
        ScriptCompiler::Compile(context, &source, options).ToLocalChecked() ;
    ScriptCompiler::CachedData* cache =
        ScriptCompiler::CreateCodeCache(script->GetUnboundScript()) ;

    i::WriteBytes(
        ChangeFileExtension(script_path, ".cmpl").c_str(),
        cache->data, cache->length, true) ;
  }

  isolate->Dispose() ;
}

}  // namespace internal
}  // namespace vm
}  // namespace v8
