// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-vm.h"
#include "vm_apps/utils/app-utils.h"
#include "vm_apps/utils/command-line.h"

const char kSwitchCommand[] = "cmd" ;
const char kSwitchCompilation[] = "cmpl" ;
const char kSwitchJSScript[] = "js" ;
const char kSwitchMode[] = "mode" ;
const char kSwitchSnapshotIn[] = "snap_i" ;
const char kSwitchSnapshotOut[] = "snap_o" ;

const char kSwitchModeCmdRun[] = "cmdrun" ;
const char kSwitchModeCompile[] = "compile" ;
const char kSwitchModeDump[] = "dump" ;
const char kSwitchModeErrorList[] = "error-list" ;

const char kCompilationFileExtension[] = ".cmpl" ;
const char kContextDumpFileExtension[] = ".context-dump.json" ;
const char kHeapDumpFileExtension[] = ".heap-dump.json" ;
const char kHeapGraphDumpFileExtension[] = ".heap-graph-dump.json" ;

enum class ModeType {
  Unknown = 0,
  Compile,
  Run,
  Dump,
  ErrorList,
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
    } else if (
        cmd_line.GetSwitchValueNative(kSwitchMode) == kSwitchModeDump &&
        cmd_line.GetArgCount() != 0) {
      result = ModeType::Dump ;
    } else if (
        cmd_line.GetSwitchValueNative(kSwitchMode) == kSwitchModeErrorList) {
      result = ModeType::ErrorList ;
    }
  }

  return result ;
}

int DoUnknown() {
  const char usage[] =
      "usage: v8_vm --mode=<mode_type> <args>\n\n"
      "These are mode types and appropriate arguments:\n"
      "  mode=compile     Compile js-file(s)\n"
      "    <args>         js-file path(s) (may be more than one)\n"
      "  e.g.: v8_vm --mode=compile script.js\n\n"
      "  mode=cmdrun      Run a js-file in environment (one of environment arguments must be)\n"
      "    cmd=<path>     Js-file path for running (must be)\n"
      "    snap_i=<path>  Snapshot of environment\n"
      "    cmpl=<path>    Compilation of js-script for creating environment (Be ignored if \'snap_i\' is present)\n"
      "    js=<path>      Js-script for creating environment (Be ignored if \'snap_i\' or \'cmpl\' are present)\n"
      "    snap_o=<path>  Path for saving environment after script has been run (optional)\n"
      "  e.g.: v8_vm --mode=cmdrun --cmd=script_cmd.js --js=script.js --snap_o=script.shot\n"
      "        v8_vm --mode=cmdrun --cmd=script_cmd.js --cmpl=script.cmpl\n"
      "        v8_vm --mode=cmdrun --cmd=script_cmd.js --snap_i=script1.shot --snap_o=script2.shot\n\n"
      "  mode=dump        Create a dump by snapshot-file(s)\n"
      "    <args>         snapshot-file path(s) (may be more than one)\n"
      "  e.g.: v8_vm --mode=dump script.shot\n\n"
      "  mode=error-list  Trace a error list\n"
      "  e.g.: v8_vm --mode=error-list" ;
  std::string common_switches = GetCommonCommandLineSwitches() ;
  printf("%s\n\n%s\n", usage, common_switches.c_str()) ;
  return 1 ;
}

int DoCompile(const CommandLine& cmd_line) {
  bool error = false ;
  for (auto it : cmd_line.GetArgs()) {
    Error result = CompileScriptFromFile(
        it.c_str(),
        ChangeFileExtension(it.c_str(), kCompilationFileExtension).c_str()) ;
    if (V8_ERROR_FAILED(result)) {
      V8_LOG_ERR(result, "File \'%s\' hasn't been compiled", it.c_str()) ;
      error = true ;
    }
  }

  return (error ? errIncompleteOperation : 0) ;
}

int DoRun(const CommandLine& cmd_line) {
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

  Error result = errOk ;
  if (snapshot_path.length()) {
    result = RunScriptBySnapshotFromFile(
        snapshot_path.c_str(),
        cmd_line.GetSwitchValueNative(kSwitchCommand).c_str(),
        out_snapshot_path.length() ? out_snapshot_path.c_str() : nullptr) ;
  } else if (compilation_path.length()) {
    result = RunScriptByCompilationFromFile(
        compilation_path.c_str(),
        cmd_line.GetSwitchValueNative(kSwitchCommand).c_str(),
        out_snapshot_path.length() ? out_snapshot_path.c_str() : nullptr) ;
  } else if (js_path.length()) {
    result = RunScriptByJSScriptFromFile(
        js_path.c_str(), cmd_line.GetSwitchValueNative(kSwitchCommand).c_str(),
        out_snapshot_path.length() ? out_snapshot_path.c_str() : nullptr) ;
  } else {
    return DoUnknown() ;
  }

  if (V8_ERROR_FAILED(result)) {
    V8_LOG_ERR(
        result, "Run of a command script is failed. (File: %s)",
        cmd_line.GetSwitchValueNative(kSwitchCommand).c_str()) ;
  }

  return (result != errOk ? result : 0) ;
}

int DoDump(const CommandLine& cmd_line) {
  Error result = errOk ;
  bool error_flag = false ;
  for (auto it : cmd_line.GetArgs()) {
    result = CreateContextDumpBySnapshotFromFile(
        it.c_str(), FormattedJson::True,
        ChangeFileExtension(it.c_str(), kContextDumpFileExtension).c_str()) ;
    if (V8_ERROR_FAILED(result)) {
      error_flag = true ;
      V8_LOG_ERR(
          result,
          "CreateContextDumpBySnapshotFromFile is failed - the file is \'%s\'",
          it.c_str()) ;
    }

    result = CreateHeapDumpBySnapshotFromFile(
        it.c_str(),
        ChangeFileExtension(it.c_str(), kHeapDumpFileExtension).c_str()) ;
    if (V8_ERROR_FAILED(result)) {
      error_flag = true ;
      V8_LOG_ERR(
          result,
          "CreateHeapDumpBySnapshotFromFile is failed - the file is \'%s\'",
          it.c_str()) ;
    }

    result = CreateHeapGraphDumpBySnapshotFromFile(
        it.c_str(), FormattedJson::True,
        ChangeFileExtension(it.c_str(), kHeapGraphDumpFileExtension).c_str()) ;
    if (V8_ERROR_FAILED(result)) {
      error_flag = true ;
      V8_LOG_ERR(
          result,
          "CreateHeapGraphDumpBySnapshotFromFile is failed - "
          "the file is \'%s\'",
          it.c_str()) ;
    }
  }

  return (error_flag ? errIncompleteOperation : 0) ;
}

int DoErrorList() {
  printf("%-32s%-12s%s", "Name", "Id", "Description\n") ;

  Error error = errOk ;
  #define V8_WARNING(type_name, prefix_flag, wrn_name, ...) \
    error = V8_ERROR_WARNING_NAME(type_name, prefix_flag, wrn_name) ; \
    printf("%-32s0x%08x  \"%s\"\n", error.name(), error.code(), \
           error.description()) ;
  #include "include/v8-vm-error-list.h"
  #undef V8_WARNING
  // Errors
  #define V8_ERROR(type_name, prefix_flag, err_name, ...) \
    error = V8_ERROR_ERROR_NAME(type_name, prefix_flag, err_name) ; \
    printf("%-32s0x%08x  \"%s\"\n", error.name(), error.code(), \
           error.description()) ;
  #include "include/v8-vm-error-list.h"
  #undef V8_ERROR

  return 0 ;
}

int main(int argc, char* argv[]) {
  CommandLine cmd_line(argc, argv) ;

  ModeType mode_type = GetModeType(cmd_line) ;
  std::unique_ptr<V8Initializer> v8_initializer ;
  if (mode_type != ModeType::Unknown) {
    // Initialize V8
    v8_initializer.reset(new V8Initializer(cmd_line, &argc, argv)) ;
  }

  int result = 0 ;
  if (mode_type == ModeType::Unknown) {
    result = DoUnknown() ;
  } else if (mode_type == ModeType::Compile) {
    result = DoCompile(cmd_line) ;
  } else if (mode_type == ModeType::Run) {
    result = DoRun(cmd_line) ;
  } else if (mode_type == ModeType::Dump) {
    result = DoDump(cmd_line) ;
  } else if (mode_type == ModeType::ErrorList) {
    result = DoErrorList() ;
  }

  return result ;
}
