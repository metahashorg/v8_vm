// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_SCRIPT_RUNNER_H_
#define V8_VM_SCRIPT_RUNNER_H_

#include "include/v8-vm.h"
#include "src/base/macros.h"
#include "src/vm/utils/vm-utils.h"
#include "src/vm/work-context.h"

namespace v8 {
namespace vm {
namespace internal {

class ScriptRunner {
 public:
  // Destructor
  ~ScriptRunner() ;

  // Run a command script (can be called many times)
  Error Run() ;

  static Error Create(
      const Data* data, const Data& script,
      std::unique_ptr<ScriptRunner>& runner,
      StartupData* snapshot_out = nullptr) ;

  static Error CreateByFiles(
      Data::Type file_type, const char* file_path, const char* script_path,
      std::unique_ptr<ScriptRunner>& runner,
      StartupData* snapshot_out = nullptr) ;

 private:
  ScriptRunner(StartupData* snapshot, StartupData* snapshot_out) ;

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
