// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-vm.h"

#include <fstream>
#include <iostream>
#include <mutex>
#include <deque>
#include <thread>

#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "src/utils.h"
#include "src/vm/dumper.h"
#include "src/vm/script-runner.h"
#include "src/vm/utils/string-printf.h"
#include "src/vm/utils/vm-utils.h"
#include "src/vm/v8-handle.h"
#include "src/vm/vm-compiler.h"

namespace vi = v8::vm::internal ;
using OS = v8::base::OS ;
using Time = v8::base::Time ;
using TimeDelta = v8::base::TimeDelta ;

namespace v8 {
namespace vm {

namespace {

#if defined(V8_VM_USE_LOG)

class Logger {
  // Log message
  struct Message {
    Time time = Time::Now() ;
    int thread_id = OS::GetCurrentThreadId() ;
    LogLevels log_level ;
    std::string message ;
  };

 public:
  // Initializes a log
  static void InitializeLog(
    LogLevels log_level, const char* log_path, const char* file_prefix,
    std::int32_t log_file_size, bool stdout_flag, bool stderr_flag) ;

  // Deinitializes the log
  static void DeinitializeLog() ;

  // Prints a message into the log
  static void PrintLogMessage(
      LogLevels log_level, const Error* error, const char* file,
      std::int32_t line, const char* msg, va_list ap) PRINTF_FORMAT(5, 0) ;

 private:
  // Constructor
  Logger(
      LogLevels log_level, const char* log_path, const char* file_prefix,
      std::int32_t log_file_size, bool stdout_flag, bool stderr_flag) ;

  // Destructor
  ~Logger() ;

  // Prints a header of the log
  void PrintHeader() ;

  // Prints a footer of the log
  void PrintFooter() ;

  // Puts a message into a message queue
  void PrintMessage(
      LogLevels log_level, const Error* error, const char* file,
      std::int32_t line, const char* msg, va_list ap) PRINTF_FORMAT(6, 0) ;

  // Prints a message into the log
  void PrintMessage(const Message& msg) ;

  // Main function of a log thread
  void Run() ;

  // Delimiter of message fields
  std::string field_delimiter_ ;

  std::unique_ptr<std::thread> thread_ ;
  Time beginning_time_ = Time::Now() ;
  std::int32_t log_id_ ;
  std::atomic<LogLevels> log_level_ ;
  std::string log_path_ ;
  std::string file_prefix_ ;
  std::int32_t log_file_size_ ;
  bool stdout_flag_ ;
  bool stderr_flag_ ;
  std::deque<Message> messages_ ;
  std::mutex message_lock_ ;
  std::condition_variable message_cv_ ;

  // Instance of a logger
  static std::unique_ptr<Logger> instance_ ;

  template<class T> friend struct std::default_delete ;
  template<class T, class D> friend class std::unique_ptr ;
};

std::unique_ptr<Logger> Logger::instance_ = nullptr ;

void Logger::InitializeLog(
  LogLevels log_level, const char* log_path, const char* file_prefix,
  std::int32_t log_file_size, bool stdout_flag, bool stderr_flag) {
  if (instance_) {
    DeinitializeLog() ;
  }

  if (log_level == LogLevels::None ||
      ((!log_path || !strlen(log_path)) && !stdout_flag && !stderr_flag)) {
    return ;
  }

  instance_.reset(new Logger(
      log_level, log_path, file_prefix, log_file_size, stdout_flag,
      stderr_flag)) ;
}

void Logger::DeinitializeLog() {
  instance_.reset() ;
}

void Logger::PrintLogMessage(
    LogLevels log_level, const Error* error, const char* file,
    std::int32_t line, const char* msg, va_list ap) {
  if (!instance_ || log_level > instance_->log_level_) {
    return ;
  }

  instance_->PrintMessage(log_level, error, file, line, msg, ap) ;
}

Logger::Logger(
    LogLevels log_level, const char* log_path, const char* file_prefix,
    std::int32_t log_file_size, bool stdout_flag, bool stderr_flag)
  : field_delimiter_(" ") , log_level_(log_level),
    log_path_(log_path ? log_path : ""),
    file_prefix_(file_prefix ? file_prefix : ""), log_file_size_(log_file_size),
    stdout_flag_(stdout_flag), stderr_flag_(stderr_flag) {
  USE(log_file_size_) ;
  // Generate a log id
  log_id_ = static_cast<std::int32_t>(std::time(nullptr)) ;
  // Start a server thread
  thread_.reset(new std::thread(&Logger::Run, std::ref(*this))) ;
  // Print a header of the log
  PrintHeader() ;
}

Logger::~Logger() {
  // Print a footer of the log
  PrintFooter() ;
  // Set flag of stopping a log
  log_level_ = LogLevels::None ;
  // Stop and wait a logger thread
  if (thread_.get() && thread_->joinable()) {
    message_cv_.notify_all() ;
    thread_->join() ;
  }

  thread_.reset() ;
}

void Logger::PrintHeader() {
  static const char header_template[] =
      "============================== Log:%08X - The beginning "
      "(%04d-%02d-%02d %02d:%02d:%02d) ==============================\n"
      "Time                      %sThread id %sLevel%sMessage" ;

  Time::Exploded time ;
  beginning_time_.LocalExplode(&time) ;
  Message msg{
      .log_level = LogLevels::Message,
      .message = vi::StringPrintf(
          header_template, log_id_, time.year, time.month, time.day_of_month,
          time.hour, time.minute, time.second, field_delimiter_.c_str(),
          field_delimiter_.c_str(), field_delimiter_.c_str()) } ;
  std::unique_lock<std::mutex> locker(message_lock_) ;
  messages_.emplace_back(std::move(msg)) ;
  message_cv_.notify_all() ;
}

void Logger::PrintFooter() {
  static const char header_template[] =
      "============================== Log:%08X - The end "
      "(%04d-%02d-%02d %02d:%02d:%02d) ==============================" ;

  Time::Exploded time ;
  Time::Now().LocalExplode(&time) ;
  Message msg{
      .log_level = LogLevels::Message,
      .message = vi::StringPrintf(
          header_template, log_id_, time.year, time.month, time.day_of_month,
          time.hour, time.minute, time.second) } ;
  std::unique_lock<std::mutex> locker(message_lock_) ;
  messages_.emplace_back(std::move(msg)) ;
  message_cv_.notify_all() ;
}

void Logger::PrintMessage(
    LogLevels log_level, const Error* error, const char* file,
    std::int32_t line, const char* msg, va_list ap) {
  static const char message_file[] = " (File:%s Line:%d)" ;
  static const char message_error[] = " (Error:%s(0x%08x))" ;
  static const char message_file_error[] =
      " (Error:%s(0x%08x) File:%s Line:%d)" ;

  std::string msg_suffix ;
  if (log_level > LogLevels::Message) {
    if (file && error) {
      msg_suffix = vi::StringPrintf(
          message_file_error, error->name(), error->code(), file, line) ;
    } else if (file) {
      msg_suffix = vi::StringPrintf(message_file, file, line) ;
    } else if (error) {
      msg_suffix = vi::StringPrintf(
          message_error, error->name(), error->code()) ;
    }
  }

  Message msg_obj{
      .log_level = log_level,
      .message = vi::StringPrintV(msg, ap) + msg_suffix } ;
  std::unique_lock<std::mutex> locker(message_lock_) ;
  messages_.emplace_back(std::move(msg_obj)) ;
  message_cv_.notify_all() ;
}

void Logger::PrintMessage(const Message& msg) {
  static const char full_msg_template[] =
      "%04d-%02d-%02d %02d:%02d:%02d.%06d" "%s" "0x%08x" "%s" "%s" "%s" "%s\n" ;

  // Create a message
  std::string msg_str ;
  if (msg.log_level > LogLevels::Message) {
    // Get time of the message
    Time::Exploded time ;
    msg.time.LocalExplode(&time) ;
    // Get a message type as string (the length equates five symbols)
    const char* msg_type = "NONE " ;
    switch (msg.log_level) {
      case LogLevels::Error: msg_type = "ERROR" ; break ;
      case LogLevels::Warning: msg_type = "WARN " ; break ;
      case LogLevels::Info: msg_type = "INFO " ; break ;
      case LogLevels::Verbose: msg_type = "VERBS" ; break ;
      default: break ;
    };

    msg_str = vi::StringPrintf(
        full_msg_template, time.year, time.month, time.day_of_month, time.hour,
        time.minute, time.second, time.microsecond, field_delimiter_.c_str(),
        msg.thread_id, field_delimiter_.c_str(), msg_type,
        field_delimiter_.c_str(), msg.message.c_str()) ;
  } else {
    msg_str = msg.message + "\n" ;
  }

  if (stdout_flag_) {
    std::cout << msg_str ;
  }

  if (stderr_flag_ && msg.log_level <= LogLevels::Warning) {
    std::cerr << msg_str ;
  }
}

void Logger::Run() {
  for (;;) {
    // Get a message queue
    std::deque<Message> messages ;
    {
      std::unique_lock<std::mutex> locker(message_lock_) ;
      if (messages_.empty() && log_level_ != LogLevels::None) {
        message_cv_.wait(locker) ;
      }

      messages.swap(messages_) ;
    }

    // Check on stopping
    if (messages.empty() && log_level_ == LogLevels::None) {
      break ;
    }

    // Print messages
    for (auto it = messages.begin(); it != messages.end(); ++it) {
      PrintMessage(*it) ;
    }
  }

  if (stdout_flag_) {
    std::cout.flush() ;
  }

  if (stderr_flag_) {
    std::cerr.flush() ;
  }
}

#endif  // defined(V8_VM_USE_LOG)

enum class DumpType {
  Context = 1,
  Heap = 2,
  HeapGraph = 3,
};

void CreateDumpBySnapshotFromFile(
    DumpType type, const char* snapshot_path, v8::vm::FormattedJson formatted,
    const char* result_path) {
  vi::Data data(vi::Data::Type::Snapshot, snapshot_path) ;
  data.data = reinterpret_cast<const char*>(
    i::ReadBytes(snapshot_path, &data.size, true)) ;
  data.owner = true ;
  if (data.size == 0) {
    printf("ERROR: Snapshot is corrupted (\'%s\')\n", snapshot_path) ;
    return ;
  }

  std::fstream fs(result_path, std::fstream::out | std::fstream::binary) ;
  if (fs.fail()) {
    printf("ERROR: Can't open file \'%s\'\n", result_path) ;
    return ;
  }

  StartupData snapshot = { data.data, data.size } ;
  std::unique_ptr<vi::WorkContext> context(vi::WorkContext::New(&snapshot)) ;
  if (type == DumpType::Context) {
    CreateContextDump(*context, fs, formatted) ;
  } else if (type == DumpType::Heap) {
    CreateHeapDump(*context, fs) ;
  } else {
    CreateHeapGraphDump(*context, fs, formatted) ;
  }

  fs.close() ;

  printf(
      "INFO: Created a dump by the snapshot-file \'%s\' and "
      "saved result into \'%s\'\n",
      snapshot_path, result_path) ;
}

Error RunScriptByFile(
    vi::Data::Type file_type, const char* file_path, const char* script_path,
    const char* snapshot_out_path /*= nullptr*/) {
  StartupData data = { nullptr, 0 }, *pdata = nullptr ;
  bool save_snapshot = (snapshot_out_path && strlen(snapshot_out_path)) ;
  if (save_snapshot) {
    pdata = &data ;
  }

  std::unique_ptr<vi::ScriptRunner> runner ;
  Error result = vi::ScriptRunner::CreateByFiles(
      file_type, file_path, script_path, runner, pdata) ;
  if (V8_ERROR_FAILED(result)) {
    printf("ERROR: Can't create ScriptRunner\n") ;
    return result ;
  }

  result = runner->Run() ;
  if (V8_ERROR_FAILED(result)) {
    printf("ERROR: Script run is failed\n") ;
    return result ;
  }

  // We've obtained a snapshot only after destruction of runner
  runner.reset() ;
  if (save_snapshot) {
    i::WriteChars(snapshot_out_path, data.data, data.raw_size, true) ;
    if (data.raw_size) {
      delete [] data.data ;
    }
  }

  return errOk ;
}

}  // namespace

std::size_t Error::message_count() const {
  return (messages_ ? messages_->size() : 0) + 1 ;
}

const Error::Message& Error::message(std::size_t index) const {
  std::size_t message_count = (messages_ ? messages_->size() : 0) ;

  if (error_message_position_ > message_count) {
    error_message_position_ = message_count ;
  }

  if (index == error_message_position_ || index > message_count) {
    if (!error_message_) {
      error_message_.reset(new Message()) ;
    }

    error_message_->message = description() ;
    error_message_->file = file() ;
    error_message_->line = line() ;
    return *error_message_ ;
  }

  return (*messages_)[index < error_message_position_ ? index : index - 1] ;
}

Error& Error::AddMessage(
    const std::string& msg, const char* file, std::uint32_t line,
    std::uint32_t back_offset, bool write_log) {
  std::size_t msg_count = message_count() ;
  // Check the back offset for we don't move fixed messages
  if (back_offset > (msg_count - fixed_message_count_)) {
    back_offset = static_cast<std::uint32_t>(msg_count - fixed_message_count_) ;
  }

  // Check a position of an error message
  if ((msg_count - back_offset) <= error_message_position_) {
    ++error_message_position_ ;
    --back_offset ;
  }

  // Create queue of messages
  if (!messages_) {
    messages_ = std::make_shared<std::deque<Message>>() ;
  }

  // Find a position for insertion
  auto pos = messages_->end() ;
  for (; back_offset > 0 && pos != messages_->begin(); --back_offset, --pos) ;

  // Insert message
  messages_->insert(pos, Message{msg, file, line}) ;

  // Write log
  if (write_log) {
    if (code_ != errOk) {
      V8_LOG(
          V8_ERROR_FAILED(*this) ? LogLevels::Error : LogLevels::Warning, this,
          file, line, "%s", msg.c_str()) ;
    } else {
      V8_LOG(LogLevels::Info, nullptr, file, line, "%s", msg.c_str()) ;
    }
  }

  return *this ;
}

void Error::FixCurrentMessageQueue() {
  fixed_message_count_ = message_count() ;
}

void InitializeLog(
    LogLevels log_level, const char* log_path, const char* file_prefix,
    std::int32_t log_file_size, bool stdout_flag, bool stderr_flag) {
#if defined(V8_VM_USE_LOG)
  Logger::InitializeLog(
      log_level, log_path, file_prefix, log_file_size, stdout_flag,
      stderr_flag) ;
#endif  // defined(V8_VM_USE_LOG)
}

void DeinitializeLog() {
#if defined(V8_VM_USE_LOG)
  Logger::DeinitializeLog() ;
#endif  // defined(V8_VM_USE_LOG)
}

#if defined(V8_VM_USE_LOG)

void PrintLogMessage(
  LogLevels log_level, const Error* error, const char* file, std::int32_t line,
  const char* msg, ...) {
  va_list ap ;
  va_start(ap, msg) ;
  Logger::PrintLogMessage(log_level, error, file, line, msg, ap) ;
  va_end(ap) ;
}

FunctionBodyLog::FunctionBodyLog(
    const char* function, const char* file, std::int32_t line, bool log_flag)
  : function_(function), file_(file), line_(line), log_flag_(log_flag),
    beginning_time_(new Time(Time::Now())) {
  if (log_flag_) {
    V8_LOG(
        LogLevels::Verbose, nullptr, file_, line_, "\'%s\' - the beginning",
        function_) ;
  }
}

FunctionBodyLog::~FunctionBodyLog() {
  if (log_flag_) {
    TimeDelta delta = Time::Now() - *beginning_time_ ;
    V8_LOG(
        LogLevels::Verbose, nullptr, file_, line_, "\'%s\' - the end "
        "(Execution time: %d.%06d seconds)", function_,
        static_cast<std::int32_t>(delta.InSeconds()),
        static_cast<std::int32_t>(
            delta.InMicroseconds() % Time::kMicrosecondsPerSecond)) ;
  }
}

#endif  // defined(V8_VM_USE_LOG)

void InitializeV8(const char* app_path) {
  V8HANDLE()->Initialize(app_path) ;
}

void DeinitializeV8() {
  V8HANDLE()->Deinitialize() ;
}

Error CompileScript(
    const char* script, const char* script_origin,
    ScriptCompiler::CachedData& result) {
  vi::Data data ;
  Error res = vi::CompileScript(script, script_origin, data) ;
  if (V8_ERROR_FAILED(res)) {
    printf("ERROR: Can't compile the script\n") ;
    return res ;
  }

  // Clean a result
  if (result.data != nullptr &&
      result.buffer_policy == ScriptCompiler::CachedData::BufferOwned) {
    delete [] result.data ;
    result.data = nullptr ;
  }

  // Save a compilation into a result
  result.length = data.size ;
  result.buffer_policy = ScriptCompiler::CachedData::BufferOwned ;
  if (data.owner) {
    result.data = reinterpret_cast<const std::uint8_t*>(data.data) ;
    data.owner = false ;
  } else {
    result.data = new uint8_t[data.size] ;
    memcpy(const_cast<uint8_t*>(result.data), data.data, data.size) ;
  }

  return errOk ;
}

Error CompileScriptFromFile(const char* script_path, const char* result_path) {
  return vi::CompileScriptFromFile(script_path, result_path) ;
}

void CreateContextDumpBySnapshotFromFile(
    const char* snapshot_path, FormattedJson formatted,
    const char* result_path) {
  CreateDumpBySnapshotFromFile(
      DumpType::Context, snapshot_path, formatted, result_path) ;
}

void CreateHeapDumpBySnapshotFromFile(
    const char* snapshot_path, const char* result_path) {
  CreateDumpBySnapshotFromFile(
      DumpType::Heap, snapshot_path, FormattedJson::False, result_path) ;
}

void CreateHeapGraphDumpBySnapshotFromFile(
    const char* snapshot_path, FormattedJson formatted,
    const char* result_path) {
  CreateDumpBySnapshotFromFile(
      DumpType::HeapGraph, snapshot_path, formatted, result_path) ;
}

Error RunScript(
    const char* script, const char* script_origin /*= nullptr*/,
    StartupData* snapshot_out /*= nullptr*/) {
  V8_LOG_FUNCTION_BODY() ;

  vi::Data script_data(vi::Data::Type::JSScript, script_origin, script) ;
  std::unique_ptr<vi::ScriptRunner> runner ;
  Error result = vi::ScriptRunner::Create(
      nullptr, script_data, runner, snapshot_out) ;
  if (V8_ERROR_FAILED(result)) {
    printf("ERROR: Can't create ScriptRunner\n") ;
    return result ;
  }

  result = runner->Run() ;
  if (V8_ERROR_FAILED(result)) {
    printf("ERROR: Script run is failed\n") ;
    return result ;
  }

  // We've obtained a snapshot only after destruction of runner
  runner.reset() ;
  return result ;
}

Error RunScriptByJSScriptFromFile(
    const char* js_path, const char* script_path,
    const char* snapshot_out_path /*= nullptr*/) {
  return RunScriptByFile(
      vi::Data::Type::JSScript, js_path, script_path, snapshot_out_path) ;
}

Error RunScriptByCompilationFromFile(
    const char* compilation_path, const char* script_path,
    const char* snapshot_out_path /*= nullptr*/) {
  return RunScriptByFile(
      vi::Data::Type::Compilation, compilation_path, script_path,
      snapshot_out_path) ;
}

Error RunScriptBySnapshot(
    StartupData& snapshot, const char* script,
    const char* snapshot_origin /*= nullptr*/,
    const char* script_origin /*= nullptr*/,
    StartupData* snapshot_out /*= nullptr*/) {
  vi::Data snapshot_data(
      vi::Data::Type::Snapshot, snapshot_origin,
      snapshot.data, snapshot.raw_size) ;
  vi::Data script_data(vi::Data::Type::JSScript, script_origin, script) ;
  std::unique_ptr<vi::ScriptRunner> runner ;
  Error result = vi::ScriptRunner::Create(
      &snapshot_data, script_data, runner, snapshot_out) ;
  if (V8_ERROR_FAILED(result)) {
    printf("ERROR: Can't create ScriptRunner\n") ;
    return result ;
  }

  return runner->Run() ;
}

Error RunScriptBySnapshotFromFile(
    const char* snapshot_path, const char* script_path,
    const char* snapshot_out_path /*= nullptr*/) {
  return RunScriptByFile(
      vi::Data::Type::Snapshot, snapshot_path, script_path,
      snapshot_out_path) ;
}

}  // namespace vm
}  // namespace v8
