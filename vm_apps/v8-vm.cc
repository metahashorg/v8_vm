// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-vm.h"
#include "vm_apps/command-line.h"

const char kSwitchCmd[] = "cmd" ;
const char kSwitchCmpl[] = "cmpl" ;
const char kSwitchMode[] = "mode" ;

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
        cmd_line.GetArgCount() != 0)
      result = ModeType::Compile ;
    else if (cmd_line.GetSwitchValueNative(kSwitchMode) == kSwitchModeCmdRun &&
             cmd_line.HasSwitch(kSwitchCmd) &&
             cmd_line.HasSwitch(kSwitchCmpl))
      result = ModeType::Run ;
    else if (cmd_line.GetSwitchValueNative(kSwitchMode) == kSwitchModeDump)
      ;
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

  for (auto it : cmd_line.GetArgs())
    v8::vm::CompileScript(it.c_str()) ;

  // Deinitialize V8
  v8::vm::DeinitializeV8() ;
  return 0 ;
}

int DoRun(const CommandLine& cmd_line) {
  // Initialize V8
  v8::vm::InitializeV8(cmd_line.GetProgram().c_str()) ;

  v8::vm::RunScriptByCompilation(
      cmd_line.GetSwitchValueNative(kSwitchCmpl).c_str(),
      cmd_line.GetSwitchValueNative(kSwitchCmd).c_str()) ;

  // Deinitialize V8
  v8::vm::DeinitializeV8() ;
  return 0 ;
}

int main(int argc, char* argv[]) {
  CommandLine cmd_line(argc, argv) ;
  ModeType mode_type = GetModeType(cmd_line) ;
  int result = 0 ;
  if (mode_type == ModeType::Unknown)
    result = DoUnknown() ;
  else if (mode_type == ModeType::Compile)
    result = DoCompile(cmd_line) ;
  else if (mode_type == ModeType::Run)
    result = DoRun(cmd_line) ;

  return result ;
}
