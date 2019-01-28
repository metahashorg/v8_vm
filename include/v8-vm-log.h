// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_VM_LOG_H_
#define INCLUDE_V8_VM_LOG_H_

#include "include/v8-vm-error.h"
#include "src/base/build_config.h"
#include "src/base/compiler-specific.h"

namespace v8 {
namespace base {

class Time ;

}  // namespace base
}  // namespace v8

namespace v8 {
namespace vm {

// Enum of log levels
enum class LogLevels {
  None = 0,
  Message,
  Error,
  Warning,
  Info,
  Verbose,
};

constexpr std::int32_t kDefaultLogFileSize = 10*1024*1024 ;

// Initializes a log system
void V8_EXPORT InitializeLog(
    LogLevels log_level, const char* log_path = nullptr,
    const char* file_prefix = nullptr,
    std::int32_t log_file_size = kDefaultLogFileSize, bool stdout_flag = true,
    bool stderr_flag = false) ;

// Deinitializes a log system
void V8_EXPORT DeinitializeLog() ;

#if !defined(V8_VM_USE_LOG)

#define V8_LOG(...) ((void) 0)
#define V8_LOG_MSG(...) ((void) 0)
#define V8_LOG_ERR(...) ((void) 0)
#define V8_LOG_WRN(...) ((void) 0)
#define V8_LOG_INF(...) ((void) 0)
#define V8_LOG_VBS(...) ((void) 0)
#define V8_LOG_FUNCTION_BODY() ((void) 0)
#define V8_LOG_FUNCTION_BODY_WITH_FLAG() ((void) 0)
#define V8_LOG_RETURN return

#else  // V8_VM_USE_LOG

// Debugs log messages
// #define V8_DBG_LOG() \
//   printf("LOG DBG File:%s Line:%d\n", V8_PROJECT_FILE_NAME, __LINE__)
#define V8_DBG_LOG() ((void) 0)

// Main macros for working wtih a log
#if defined(V8_OS_WIN)

#define V8_LOG(level, error, file, line, msg, ...) \
  V8_DBG_LOG() ; \
  PrintLogMessage((level), error, file, line, msg, __VA_ARGS__)

#else  // V8_OS_WIN

#define V8_LOG(level, error, file, line, msg, ...) \
  V8_DBG_LOG() ; \
  PrintLogMessage((level), error, file, line, msg, ## __VA_ARGS__)

#endif  // V8_OS_WIN

#define V8_LOG_MSG(msg, ...) \
  V8_LOG(LogLevels::Message, nullptr, nullptr, 0, msg, __VA_ARGS__)
#define V8_LOG_ERR(error, msg, ...) \
  V8_LOG( \
      LogLevels::Error, &error, V8_PROJECT_FILE_NAME, __LINE__, \
      msg, __VA_ARGS__)
#define V8_LOG_WRN(error, msg, ...) \
  V8_LOG( \
      LogLevels::Warning, &error, V8_PROJECT_FILE_NAME, __LINE__, \
      msg, __VA_ARGS__)
#define V8_LOG_INF(msg, ...) \
  V8_LOG( \
      LogLevels::Info, nullptr, V8_PROJECT_FILE_NAME, __LINE__, \
      msg, __VA_ARGS__)
#define V8_LOG_VBS(msg, ...) \
  V8_LOG( \
      LogLevels::Verbose, nullptr, V8_PROJECT_FILE_NAME, __LINE__, \
      msg, __VA_ARGS__)

// Macros for tracing a function body
#define V8_LOG_FUNCTION_BODY() V8_LOG_FUNCTION_BODY_WITH_FLAG(true)
#define V8_LOG_FUNCTION_BODY_WITH_FLAG(flag) \
  v8::vm::FunctionBodyLog v8_function_body_log( \
      V8_FUNCTION, V8_PROJECT_FILE_NAME, __LINE__, (flag))
#define V8_LOG_RETURN v8_function_body_log.SetLine(__LINE__) ; return

// Prints a message into a log
void V8_EXPORT PrintLogMessage(
    LogLevels log_level, const Error* error, const char* file,
    std::int32_t line, _Printf_format_string_ const char* msg, ...)
    PRINTF_FORMAT(5, 6) ;

// Prints a log message on the beginning and the end of function
class V8_EXPORT FunctionBodyLog {
 public:
  FunctionBodyLog(
      const char* function, const char* file, std::int32_t line,
      bool log_flag) ;
  ~FunctionBodyLog() ;
  void SetLine(std::int32_t line) { line_ = line ; }
 private:
  const char* function_ ;
  const char* file_ ;
  std::int32_t line_ ;
  bool log_flag_ ;
  std::unique_ptr<base::Time> beginning_time_ ;
};

#endif  // V8_VM_USE_LOG

}  // namespace vm
}  // namespace v8

#endif  // INCLUDE_V8_VM_ERROR_H_
