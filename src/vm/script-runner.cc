// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/vm/script-runner.h"

#include "src/utils.h"
#include "src/utils.h"
#include "src/vm/v8-handle.h"
#include "src/vm/vm-compiler.h"
#include "src/vm/vm-utils.h"

namespace v8 {
namespace vm {
namespace internal {

static Local<Module> global_main_module ;
v8::MaybeLocal<Module> ModuleResolveCallback(
    Local<Context> context, Local<String> specifier, Local<Module> referrer) {
  // TODO: Temporary output
  v8::String::Utf8Value utf8(context->GetIsolate(), specifier) ;
  printf("Specifier: %s\n", *utf8) ;

  return global_main_module ;
}

void RunScriptByCompilation(
    const char* compilation_path, const char* script_path) {
  bool file_exists = false ;
  i::Vector<const char> file_content =
      i::ReadFile(script_path, &file_exists, true) ;
  if (!file_exists || file_content.size() == 0) {
    return ;
  }

  // TODO: Check appropriate flags for import
  // i::FLAG_harmony_dynamic_import = true ;

  Isolate* isolate =
      Isolate::New(V8Handle::instance_.Pointer()->create_params()) ;

  {
    Isolate::Scope iscope(isolate) ;
    HandleScope scope(isolate) ;
    Local<Context> context = Context::New(isolate) ;
    Context::Scope cscope(context) ;

    // Load compilation
    Local<Module> main_module =
        LoadCompilation(isolate, context, compilation_path) ;
    if (main_module.IsEmpty()) {
      printf("ERROR: Module loading've ended failure\n") ;
      return ;
    }

    main_module->InstantiateModule(context, ModuleResolveCallback).ToChecked() ;
    global_main_module = main_module ;

    // Compile and run script
    Local<String> source_string = Utf8ToStr(isolate, file_content.start()) ;
    ScriptOrigin script_origin(
        Utf8ToStr(isolate, script_path), Local<v8::Integer>(),
        Local<v8::Integer>(), Local<v8::Boolean>(), Local<v8::Integer>(),
        Local<v8::Value>(), Local<v8::Boolean>(), Local<v8::Boolean>(),
        True(isolate) /* is_module */ ) ;
    ScriptCompiler::Source source(source_string, script_origin) ;
    ScriptCompiler::CompileOptions options = ScriptCompiler::kNoCompileOptions ;
    Local<Module> script =
        ScriptCompiler::CompileModule(isolate, &source, options).ToLocalChecked() ;
    script->InstantiateModule(context, ModuleResolveCallback).ToChecked() ;
    Local<Value> result = script->Evaluate(context).ToLocalChecked() ;
    USE(result) ;

    // TODO: Use embedded properties or a new class
    global_main_module.Clear() ;

    // TODO: Temporary output
    v8::String::Utf8Value utf8(isolate, result) ;
    printf("INFO: Result of command: %s\n", *utf8) ;
  }

  isolate->Dispose() ;
}

}  // namespace internal
}  // namespace vm
}  // namespace v8
