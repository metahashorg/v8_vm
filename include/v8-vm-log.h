// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_VM_LOG_H_
#define INCLUDE_V8_VM_LOG_H_

#include <atomic>

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
#define V8_LOG_WITH_ERROR(...) ((void) 0)
#define V8_LOG_MSG(...) ((void) 0)
#define V8_LOG_ERR(...) ((void) 0)
#define V8_LOG_WRN(...) ((void) 0)
#define V8_LOG_INF(...) ((void) 0)
#define V8_LOG_VBS(...) ((void) 0)
#define V8_LOG_FUNCTION_BODY() ((void) 0)
#define V8_LOG_FUNCTION_BODY_WITH_FLAG(...) ((void) 0)
#define V8_LOG_RETURN return
#define V8_LOG_FLUSH() ((void) 0)
#define V8_LOG_FUNCTION_LINE_REACHED() ((void) 0)
#define V8_LOG_SET_FUNCTION_LINE_REACHED_FLAG(...) ((void) 0)
#define V8_LOG_IS_FUNCTION_LINE_REACHED_ON() false

#else  // V8_VM_USE_LOG

// Debugs log messages
// #define V8_DBG_LOG() \
//   printf("LOG DBG File:%s Line:%d\n", V8_PROJECT_FILE_NAME, __LINE__)
#define V8_DBG_LOG() ((void) 0)

// Main macros for working wtih a log
#define V8_LOG(level, file, line, ...) \
  V8_DBG_LOG() ; \
  ::v8::vm::PrintLogMessage((level), file, line, __VA_ARGS__)
#define V8_LOG_WITH_ERROR(level, error, file, line, ...) \
  V8_DBG_LOG() ; \
  ::v8::vm::PrintLogMessage((level), error, file, line, __VA_ARGS__)

#define V8_LOG_MSG(...) \
  V8_LOG(::v8::vm::LogLevels::Message, nullptr, 0, "$ " __VA_ARGS__)
#define V8_LOG_ERR(error, ...) \
  V8_LOG_WITH_ERROR( \
      ::v8::vm::LogLevels::Error, error, V8_PROJECT_FILE_NAME, __LINE__, \
      __VA_ARGS__)
#define V8_LOG_ERR_WITH_FLAG(flag, error, ...) \
  if (flag) { V8_LOG_ERR(error, __VA_ARGS__) ; }
#define V8_LOG_WRN(error, ...) \
  V8_LOG_WITH_ERROR( \
      ::v8::vm::LogLevels::Warning, error, V8_PROJECT_FILE_NAME, __LINE__, \
      __VA_ARGS__)
#define V8_LOG_INF(...) \
  V8_LOG( \
      ::v8::vm::LogLevels::Info, V8_PROJECT_FILE_NAME, __LINE__, __VA_ARGS__)
#define V8_LOG_VBS(...) \
  V8_LOG( \
      ::v8::vm::LogLevels::Verbose, V8_PROJECT_FILE_NAME, __LINE__, __VA_ARGS__)

// Macros for tracing a function body
#define V8_LOG_FUNCTION_BODY() V8_LOG_FUNCTION_BODY_WITH_FLAG(true)
#define V8_LOG_FUNCTION_BODY_WITH_FLAG(flag) \
  V8_LOG_FUNCTION_BODY_WITH_FLAG_AND_MSG(flag, nullptr)
#define V8_LOG_FUNCTION_BODY_WITH_MSG(...) \
  V8_LOG_FUNCTION_BODY_WITH_FLAG_AND_MSG(true, __VA_ARGS__)
#define V8_LOG_FUNCTION_BODY_WITH_FLAG_AND_MSG(flag, ...) \
  ::v8::vm::FunctionBodyLog v8_log_function_body( \
      V8_FUNCTION, V8_PROJECT_FILE_NAME, __LINE__, (flag), __VA_ARGS__)
#define V8_LOG_RETURN v8_log_function_body.SetLine(__LINE__) ; return

// Macros for debugging a function especially if it crashes
// NOTE: Be careful it drastically lowers performance
#define V8_LOG_FLUSH() ::v8::vm::FlushLog()
#define V8_LOG_FUNCTION_LINE_REACHED() \
  V8_LOG( \
      ::v8::vm::LogLevels::Info, nullptr, 0, \
      "Reached Function:\'%s\' Line:%d File:%s", V8_FUNCTION, __LINE__, \
      V8_PROJECT_FILE_NAME) ; \
  V8_LOG_FLUSH()
#define V8_LOG_SET_FUNCTION_LINE_REACHED_FLAG(flag) \
  ::v8::vm::g_function_line_reached = flag
#define V8_LOG_IS_FUNCTION_LINE_REACHED_ON() ::v8::vm::g_function_line_reached

// Flag on/off is about a log of 'function line has been reached'
std::atomic<bool> g_function_line_reached(true) ;

// Prints a message into a log
void V8_EXPORT PrintLogMessage(
    LogLevels log_level, const char* file, std::int32_t line,
    _Printf_format_string_ const char* msg, ...)
    PRINTF_FORMAT(4, 5) ;
void V8_EXPORT PrintLogMessage(
    LogLevels log_level, const Error& error, const char* file,
    std::int32_t line, _Printf_format_string_ const char* msg, ...)
    PRINTF_FORMAT(5, 6) ;

// Flushes all streams of a log
// NOTE: Be careful it drastically lowers performance during the operation
void V8_EXPORT FlushLog() ;

// Prints a log message on the beginning and the end of function
class V8_EXPORT FunctionBodyLog {
 public:
  FunctionBodyLog(
      const char* function, const char* file, std::int32_t line,
      bool log_flag, _Printf_format_string_ const char* msg, ...)
      PRINTF_FORMAT(6, 7) ;
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
