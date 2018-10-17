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

namespace {

const int kScriptRunnerIndex = 2 ;

}  // namespace

ScriptRunner::~ScriptRunner() {
  result_.Clear() ;
  script_cache_.reset() ;
  main_module_.Clear() ;
  cscope_.reset() ;
  context_.Clear() ;
  scope_.reset() ;

  // Create a snapshot. We can't avoid it because error'll happen
  StartupData snapshot = snapshot_creator_->CreateBlob(
      SnapshotCreator::FunctionCodeHandling::kKeep) ;
  if (snapshot_out_) {
    *snapshot_out_ = snapshot ;
  } else {
    delete [] snapshot.data ;
  }

  iscope_.reset() ;
  snapshot_creator_.reset() ;
}

void ScriptRunner::Run() {
  // Get script from cache
  Local<Module> script = CompileModule(
      isolate_, script_origin_.c_str(), script_source_.start(),
      script_cache_.get()) ;
  if (script.IsEmpty()){
    printf("ERROR: Command script hasn't been compiled\n") ;
    return ;
  }

  script->InstantiateModule(context_, ModuleResolveCallback).ToChecked() ;

  // Run script
  result_ = script->Evaluate(context_).ToLocalChecked() ;

  // TODO: Temporary output
  v8::String::Utf8Value utf8(isolate_, result_) ;
  printf("INFO: Result of command: %s\n", *utf8) ;
}

ScriptRunner* ScriptRunner::CreateByCompilation(
    const char* compilation_path, const char* script_path,
    StartupData* snapshot_out /*= nullptr*/) {
  // Load a command script
  bool file_exists = false ;
  i::Vector<const char> script_content =
      i::ReadFile(script_path, &file_exists, true) ;
  if (!file_exists || script_content.size() == 0) {
    printf("ERROR: Script file doesn't exist (%s)\n", script_path) ;
    return nullptr ;
  }

  // Create ScriptRunner
  ScriptRunner* result = new ScriptRunner() ;

  // Load compilation
  result->main_module_ =
      LoadCompilation(result->isolate_, compilation_path) ;
  if (result->main_module_.IsEmpty()) {
    printf("ERROR: Module loading've ended failure\n") ;
    delete result ;
    return nullptr ;
  }

  result->main_module_->InstantiateModule(
      result->context_, ModuleResolveCallback).ToChecked() ;

  // Compile a script and save a cache of it
  Local<Module> script =
      CompileModule(result->isolate_, script_path, script_content.start()) ;
  result->script_cache_.reset(ScriptCompiler::CreateCodeCache(
      script->GetUnboundModuleScript())) ;

  // Remember origin and source of script
  result->script_origin_ = script_path ;
  std::swap(result->script_source_, script_content) ;
  result->snapshot_out_ = snapshot_out ;
  return result ;
}

ScriptRunner* ScriptRunner::CreateBySnapshot(
    const char* snapshot_path, const char* script_path,
    StartupData* snapshot_out /*= nullptr*/) {
  // Load a command script
  bool file_exists = false ;
  i::Vector<const char> script_content =
      i::ReadFile(script_path, &file_exists, true) ;
  if (!file_exists || script_content.size() == 0) {
    printf("ERROR: Script file doesn't exist (%s)\n", script_path) ;
    return nullptr ;
  }

  // Load a snapshot
  i::Vector<const char> snapshot_content =
      i::ReadFile(snapshot_path, &file_exists, true) ;
  if (!file_exists || snapshot_content.size() == 0) {
    printf("ERROR: Snapshot file doesn't exist (%s)\n", snapshot_path) ;
    return nullptr ;
  }

  StartupData snapshot = { nullptr, 0 } ;
  snapshot.data = snapshot_content.start() ;
  snapshot.raw_size = (int)snapshot_content.size() ;

  // Create ScriptRunner
  ScriptRunner* result = new ScriptRunner(&snapshot) ;

  delete result ;
  return nullptr ;
}

ScriptRunner::ScriptRunner(StartupData* snapshot /*= nullptr*/)
  : snapshot_creator_(new SnapshotCreator(
        V8HANDLE()->create_params().external_references, snapshot)),
    isolate_(snapshot_creator_->GetIsolate()),
    iscope_(new Isolate::Scope(isolate_)),
    scope_(new InitializedHandleScope(isolate_)),
    context_(Context::New(isolate_)),
    cscope_(new Context::Scope(context_)) {
  snapshot_creator_->SetDefaultContext(context_) ;
  // We need to pass 'this' to 'ModuleResolveCallback()''
  context_->SetAlignedPointerInEmbedderData(kScriptRunnerIndex, this) ;
}

MaybeLocal<Module> ScriptRunner::ModuleResolveCallback(
    Local<Context> context, Local<String> specifier, Local<Module> referrer) {
  // TODO: Temporary output
  v8::String::Utf8Value utf8(context->GetIsolate(), specifier) ;
  printf("INFO: Module specifier: %s\n", *utf8) ;

  ScriptRunner* runner = static_cast<ScriptRunner*>(
      context->GetAlignedPointerFromEmbedderData(kScriptRunnerIndex));
  return runner->main_module_ ;
}

}  // namespace internal
}  // namespace vm
}  // namespace v8
