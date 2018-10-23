// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-vm.h"
#include "vm_apps/command-line.h"

const char kSwitchCommand[] = "cmd" ;
const char kSwitchCompilation[] = "cmpl" ;
const char kSwitchJSScript[] = "js" ;
const char kSwitchMode[] = "mode" ;
const char kSwitchSnapshotIn[] = "snap_i" ;
const char kSwitchSnapshotOut[] = "snap_o" ;

const char kSwitchModeCmdRun[] = "cmdrun" ;
const char kSwitchModeCompile[] = "compile" ;
const char kSwitchModeDump[] = "dump" ;

enum class ModeType {
  Unknown = 0,
  Compile,
  Run,
  Dump,
};

ModeType GetModeType(const CommandLine& cmd_line) {
  ModeType result = ModeType::Unknown ;

  if (cmd_line.HasSwitch(kSwitchMode)) {
    if (cmd_line.GetSwitchValueNative(kSwitchMode) == kSwitchModeCompile &&
        cmd_line.GetArgCount() != 0) {
      result = ModeType::Compile ;
    } else if (
        cmd_line.GetSwitchValueNative(kSwitchMode) == kSwitchModeCmdRun &&
        cmd_line.HasSwitch(kSwitchCommand) &&
        (cmd_line.HasSwitch(kSwitchJSScript) ||
         cmd_line.HasSwitch(kSwitchCompilation) ||
         cmd_line.HasSwitch(kSwitchSnapshotIn))) {
      result = ModeType::Run ;
    } else if (cmd_line.GetSwitchValueNative(kSwitchMode) == kSwitchModeDump) {
      ;
    }
  }

  return result ;
}

int DoUnknown() {
  printf("Specify what you want to do\n") ;
  return 1 ;
}

int DoCompile(const CommandLine& cmd_line) {
  // Initialize V8
  v8::vm::InitializeV8(cmd_line.GetProgram().c_str()) ;

  for (auto it : cmd_line.GetArgs()) {
    v8::vm::CompileScript(it.c_str()) ;
  }

  // Deinitialize V8
  v8::vm::DeinitializeV8() ;
  return 0 ;
}

int DoRun(const CommandLine& cmd_line) {
  // Initialize V8
  v8::vm::InitializeV8(cmd_line.GetProgram().c_str()) ;

  std::string js_path ;
  if (cmd_line.HasSwitch(kSwitchJSScript)) {
    js_path = cmd_line.GetSwitchValueNative(kSwitchJSScript) ;
  }

  std::string compilation_path ;
  if (cmd_line.HasSwitch(kSwitchCompilation)) {
    compilation_path = cmd_line.GetSwitchValueNative(kSwitchCompilation) ;
  }

  std::string snapshot_path ;
  if (cmd_line.HasSwitch(kSwitchSnapshotIn)) {
    snapshot_path = cmd_line.GetSwitchValueNative(kSwitchSnapshotIn) ;
  }

  std::string out_snapshot_path ;
  if (cmd_line.HasSwitch(kSwitchSnapshotOut)) {
    out_snapshot_path = cmd_line.GetSwitchValueNative(kSwitchSnapshotOut) ;
  }

  if (snapshot_path.length()) {
    v8::vm::RunScriptBySnapshotFromFile(
        snapshot_path.c_str(),
        cmd_line.GetSwitchValueNative(kSwitchCommand).c_str(),
        out_snapshot_path.length() ? out_snapshot_path.c_str() : nullptr) ;
  } else if (compilation_path.length()) {
    v8::vm::RunScriptByCompilationFromFile(
        compilation_path.c_str(),
        cmd_line.GetSwitchValueNative(kSwitchCommand).c_str(),
        out_snapshot_path.length() ? out_snapshot_path.c_str() : nullptr) ;
  } else if (js_path.length()) {
    v8::vm::RunScriptByJSScriptFromFile(
        js_path.c_str(), cmd_line.GetSwitchValueNative(kSwitchCommand).c_str(),
        out_snapshot_path.length() ? out_snapshot_path.c_str() : nullptr) ;
  } else {
    return DoUnknown() ;
  }

  // Deinitialize V8
  v8::vm::DeinitializeV8() ;
  return 0 ;
}

int main(int argc, char* argv[]) {
  CommandLine cmd_line(argc, argv) ;
  ModeType mode_type = GetModeType(cmd_line) ;
  int result = 0 ;
  if (mode_type == ModeType::Unknown) {
    result = DoUnknown() ;
  } else if (mode_type == ModeType::Compile) {
    result = DoCompile(cmd_line) ;
  } else if (mode_type == ModeType::Run) {
    result = DoRun(cmd_line) ;
  }

  return result ;
}
