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

LogLevels StrToLogLevel(const std::string& str) {
  if (EqualsCaseInsensitiveASCII(str, "msg")) {
    return LogLevels::Message ;
  } else if (EqualsCaseInsensitiveASCII(str, "err")) {
    return LogLevels::Error ;
  } else if (EqualsCaseInsensitiveASCII(str, "wrn")) {
    return LogLevels::Warning ;
  } else if (EqualsCaseInsensitiveASCII(str, "inf")) {
    return LogLevels::Info ;
  } else if (EqualsCaseInsensitiveASCII(str, "vbs")) {
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
