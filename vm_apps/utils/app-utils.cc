// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/utils/app-utils.h"

#include <cstdio>
#include <cwchar>
#include <locale>

#include "src/base/macros.h"
#include "vm_apps/utils/string-number-conversions.h"

namespace {

const char kSwitchLogLevel[] = "log-level" ;
const char kSwitchLogPath[] = "log-path" ;
const char kSwitchLogFileSize[] = "log-file-size" ;
const char kSwitchLogStdout[] = "log-stdout" ;
const char kSwitchLogStderr[] = "log-stderr" ;

const char kLogLevelNone[] = "none" ;
const char kLogLevelMessage[] = "msg" ;
const char kLogLevelError[] = "err" ;
const char kLogLevelWarning[] = "wrn" ;
const char kLogLevelInfo[] = "inf" ;
const char kLogLevelVerbose[] = "vbs" ;

LogLevels StrToLogLevel(const std::string& str) {
  if (EqualsCaseInsensitiveASCII(str, kLogLevelMessage)) {
    return LogLevels::Message ;
  } else if (EqualsCaseInsensitiveASCII(str, kLogLevelError)) {
    return LogLevels::Error ;
  } else if (EqualsCaseInsensitiveASCII(str, kLogLevelWarning)) {
    return LogLevels::Warning ;
  } else if (EqualsCaseInsensitiveASCII(str, kLogLevelInfo)) {
    return LogLevels::Info ;
  } else if (EqualsCaseInsensitiveASCII(str, kLogLevelVerbose)) {
    return LogLevels::Verbose ;
  }

  return LogLevels::None ;
}

}  // namespace

std::string ChangeFileExtension(
    const char* file_name, const char* new_extension) {
  FilePath file_path(file_name) ;
  return file_path.ReplaceExtension(new_extension).value() ;
}

std::string GetCommonCommandLineSwitches() {
  std::string result = "Common switches that can be use in any case:\n" ;

  // Log switches
  result += "  Log switches:\n" ;
  result += StringPrintf(
      "    %-14s a log level. Available values:\n", kSwitchLogLevel) ;
  result += StringPrintf("      %-4s  a log is off\n", kLogLevelNone) ;
  result += StringPrintf(
      "      %-4s  log the only crucial messages\n", kLogLevelMessage) ;
  result += StringPrintf(
      "      %-4s  log crucial and error messages\n", kLogLevelError) ;
  result += StringPrintf(
      "      %-4s  log previous and warning messages\n", kLogLevelWarning) ;
  result += StringPrintf(
      "      %-4s  log previous and informative messages\n", kLogLevelInfo) ;
  result += StringPrintf(
      "      %-4s  log all messages\n", kLogLevelVerbose) ;
  result += StringPrintf(
      "    %-14s turns on logging into file and set paths of log files\n",
      kSwitchLogPath) ;
  result += StringPrintf(
      "    %-14s sets maximum of log file size (in bytes)\n",
      kSwitchLogFileSize) ;
  result += StringPrintf(
      "    %-14s sets flag of printing a log into stdout [true|false]\n",
      kSwitchLogStdout) ;
  result += StringPrintf(
      "    %-14s sets flag of printing a log into stderr [true|false] "
      "(the only crucial, error and warning messages will be printed)\n",
      kSwitchLogStderr) ;
  result += StringPrintf(
      "  e.g.: ... --%s=inf --%s=log --%s=100000 --%s=true ...\n",
      kSwitchLogLevel, kSwitchLogPath, kSwitchLogFileSize, kSwitchLogStdout) ;
  result +="\n" ;

  // V8 switches
  result += "  V8 switches:\n" ;
  #define FLAG_FULL(ftype, ctype, nam, def, cmt) \
      result += StringPrintf("    %-43s %s (type:%s default:%s)\n", #nam, cmt, #ctype, #def) ;
  #include "src/flag-definitions.h"
  #undef FLAG_FULL

  result +=
      "  The following syntax for V8 switches is accepted "
          "(both '-' and '--' are ok):\n"
      "    --flag        (bool flags only)\n"
      "    --noflag      (bool flags only)\n"
      "    --flag=value  (non-bool flags only, no spaces around '=')\n"
      "  e.g.: ... --heap_snapshot_string_limit=1024 --lazy --noprof ..." ;

  return result ;
}

V8Initializer::V8Initializer(
    const CommandLine& cmd_line, int* argc, char** argv) {
#ifdef DEBUG
  LogLevels log_level = LogLevels::Verbose ;
#else
  LogLevels log_level = LogLevels::None ;
#endif  // DEBUG
  if (cmd_line.HasSwitch(kSwitchLogLevel)) {
    log_level = StrToLogLevel(cmd_line.GetSwitchValueNative(kSwitchLogLevel)) ;
  }

  std::string log_path ;
  if (cmd_line.HasSwitch(kSwitchLogPath)) {
    log_path = cmd_line.GetSwitchValueNative(kSwitchLogPath) ;
  }

  std::string log_file_prefix =
      GetExecutablePath().BaseName().RemoveExtension().value() ;

  std::int32_t log_file_size = kDefaultLogFileSize ;
  if (cmd_line.HasSwitch(kSwitchLogFileSize)) {
    if (!StringToInt32(
             cmd_line.GetSwitchValueNative(kSwitchLogFileSize),
             &log_file_size) ||
        log_file_size <= 0) {
      log_file_size = kDefaultLogFileSize ;
    }
  }

  bool log_stdout = EqualsCaseInsensitiveASCII(
      cmd_line.GetSwitchValueNative(kSwitchLogStdout), "true") ;
  bool log_stderr = EqualsCaseInsensitiveASCII(
      cmd_line.GetSwitchValueNative(kSwitchLogStderr), "true") ;

  // If log level's set but we don't have any output
  // then to set stdout as output
  if (log_level != LogLevels::None && log_path.empty() &&
      !log_stdout && !log_stderr) {
    log_stdout = true ;
  }

  InitializeLog(
      log_level, log_path.c_str(), log_file_prefix.c_str(), log_file_size,
      log_stdout, log_stderr) ;
  InitializeV8(cmd_line.GetProgram().c_str(), argc, argv) ;
}

V8Initializer::~V8Initializer() {
  DeinitializeV8() ;
  DeinitializeLog() ;
}
