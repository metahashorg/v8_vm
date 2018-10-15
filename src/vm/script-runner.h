// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_SCRIPT_RUNNER_H_
#define V8_VM_SCRIPT_RUNNER_H_

#include "include/v8.h"
#include "src/base/macros.h"
#include "src/vm/vm-utils.h"

namespace v8 {
namespace vm {
namespace internal {

class ScriptRunner {
 public:
  ~ScriptRunner() ;

  void Run() ;

  static ScriptRunner* CreateByCompilation(
      const char* compilation_path, const char* script_path) ;

 private:
  ScriptRunner() ;

  static MaybeLocal<Module> ModuleResolveCallback(
      Local<Context> context, Local<String> specifier, Local<Module> referrer) ;

  std::string script_origin_ ;
  i::Vector<const char> script_source_ ;
  Isolate* isolate_ ;
  std::unique_ptr<Isolate::Scope> iscope_ ;
  std::unique_ptr<InitializedHandleScope> scope_ ;
  Local<Context> context_ ;
  std::unique_ptr<Context::Scope> cscope_ ;
  Local<Module> main_module_ ;
  std::unique_ptr<v8::ScriptCompiler::CachedData> script_cache_ ;
  Local<Value> result_ ;

  DISALLOW_COPY_AND_ASSIGN(ScriptRunner) ;
};

}  // namespace internal
}  // namespace vm
}  // namespace v8

#endif  // V8_VM_VM_COMPILER_H_
