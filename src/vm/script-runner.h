// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_SCRIPT_RUNNER_H_
#define V8_VM_SCRIPT_RUNNER_H_

#include "include/v8.h"
#include "src/base/macros.h"
#include "src/vm/vm-utils.h"
#include "src/vm/work-context.h"

namespace v8 {
namespace vm {
namespace internal {

class ScriptRunner {
 public:
  // Destructor
  ~ScriptRunner() ;

  // Run a command script (can be called many times)
  void Run() ;

  static ScriptRunner* Create(
      const Data* data, const Data& script,
      StartupData* snapshot_out = nullptr) ;

  static ScriptRunner* CreateByFiles(
      Data::Type file_type, const char* file_path, const char* script_path,
      StartupData* snapshot_out = nullptr) ;

 private:
  ScriptRunner(
      StartupData* snapshot = nullptr, StartupData* snapshot_out = nullptr) ;

  Data script_data_ ;
  std::unique_ptr<WorkContext> context_ ;
  Local<Script> main_script_ ;
  std::unique_ptr<v8::ScriptCompiler::CachedData> script_cache_ ;
  Local<Value> result_ ;

  DISALLOW_COPY_AND_ASSIGN(ScriptRunner) ;
};

}  // namespace internal
}  // namespace vm
}  // namespace v8

#endif  // V8_VM_SCRIPT_RUNNER_H_
