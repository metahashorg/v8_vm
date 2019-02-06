// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-vm.h"

#include <fstream>
#include <iostream>
#include <mutex>
#include <deque>
#include <thread>

#include "src/base/debug/stack_trace.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "src/utils.h"
#include "src/vm/dumper.h"
#include "src/vm/script-runner.h"
#include "src/vm/utils/file-path.h"
#include "src/vm/utils/string-printf.h"
#include "src/vm/utils/vm-utils.h"
#include "src/vm/v8-handle.h"
#include "src/vm/vm-compiler.h"
#include "src/vm/vm-version.h"

namespace vi = v8::vm::internal ;
using OS = v8::base::OS ;
using Time = v8::base::Time ;
using TimeDelta = v8::base::TimeDelta ;

namespace v8 {
namespace vm {

namespace {

#if defined(V8_VM_USE_LOG)

const char* LogLevelToStr(LogLevels level) {
  switch (level) {
    case LogLevels::Message:
      return "Message" ;
    case LogLevels::Error:
      return "Error" ;
    case LogLevels::Warning:
      return "Warning" ;
    case LogLevels::Info:
      return "Info" ;
    case LogLevels::Verbose:
      return "Verbose" ;
    default:
    return "None" ;
  }
}

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

  // Creates a free file name
  std::string CreateFreeFileName(const std::string& suffix = "") ;

  // Initializes a log file
  void InitializeLogFile() ;

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

  // Reopens a log file if it is more than a maximum log file size
  void UpdateLogFile() ;

  // Callback on a fatal error
  static void OnFatalError(
      const char* file, int line, const char* format, va_list args) ;

  // Callback on the process aborted
  static void OnProcessAborted() ;

  // Delimiter of message fields
  std::string field_delimiter_ ;

  std::unique_ptr<std::thread> thread_ ;
  Time beginning_time_ = Time::Now() ;
  std::int32_t log_id_ ;
  std::atomic<LogLevels> log_level_ ;
  vi::FilePath log_path_ ;
  vi::FilePath log_file_path_ ;
  std::unique_ptr<std::fstream> log_file_ ;
  std::string file_prefix_ ;
  std::int32_t max_log_file_size_ ;
  std::int32_t log_file_size_ = 0 ;
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

  // Set callback on a fatal error
  v8::base::SetFatalFunction(&OnFatalError) ;

  // Set a callback on the process aborted
  OS::AddAbortCallback(&OnProcessAborted) ;

  V8_LOG_MSG("Log level: %s", LogLevelToStr(instance_->log_level_)) ;
  if (!instance_->log_path_.empty()) {
    V8_LOG_MSG("Log path: %s", instance_->log_path_.value().c_str()) ;
  }

  V8_LOG_MSG("Log stdout: %s", instance_->stdout_flag_ ? "true" : "false") ;
  V8_LOG_MSG("Log stderr: %s", instance_->stderr_flag_ ? "true" : "false") ;
}

void Logger::DeinitializeLog() {
  // Remove callback on a fatal error
  v8::base::SetFatalFunction(nullptr) ;

  // Remove callback on the process aborted
  OS::RemoveAbortCallback(&OnProcessAborted) ;

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
    file_prefix_(file_prefix ? file_prefix : ""),
    max_log_file_size_(log_file_size), stdout_flag_(stdout_flag),
    stderr_flag_(stderr_flag) {
  // Generate a log id
  log_id_ = static_cast<std::int32_t>(std::time(nullptr)) ;
  // Initialize a log file
  InitializeLogFile() ;
  // Check that we have some output
  if (!log_file_ && !stdout_flag_ && !stderr_flag_) {
    log_level_ = LogLevels::None ;
    return ;
  }

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
  log_file_.reset() ;
}

std::string Logger::CreateFreeFileName(const std::string& suffix) {
  static const char file_name_template[] =
      "%s%08x_%04d-%02d-%02d_%02d-%02d-%02d.%06d%s.log" ;
  static const int magic_number = 73387 ;

  // Try 1000 times to find a free name
  for (int i = 0; i < 100; ++i) {
    Time::Exploded time ;
    Time::Now().LocalExplode(&time) ;
    for (int j = 0; j < 10; ++j) {
      std::string file_name = vi::StringPrintf(
          file_name_template,
          file_prefix_.empty() ? file_prefix_.c_str() :
                                 (file_prefix_ + "_").c_str(),
          log_id_, time.year, time.month, time.day_of_month, time.hour,
          time.minute, time.second, time.microsecond,
          suffix.empty() ? suffix.c_str() : ("_" + suffix).c_str()) ;
      if (!PathExists(log_path_.Append(file_name))) {
        return file_name ;
      }

      time.microsecond =
          (time.microsecond + magic_number) % Time::kMicrosecondsPerSecond ;
    }
  }

  OS::StandardOutputAutoLock locker ;
  printf("ERROR: Can't find a free name of a file\n") ;
  return "" ;
}

void Logger::InitializeLogFile() {
  if (log_path_.empty()) {
    return ;
  }

  Error result = vi::CreateDirectory(log_path_) ;
  if (V8_ERROR_FAILED(result)) {
    OS::StandardOutputAutoLock locker ;
    printf(
        "ERROR: Can't create a directory - \'%s\'\n",
        log_path_.value().c_str()) ;
    return ;
  }

  std::string file_name = CreateFreeFileName() ;
  if (file_name.empty()) {
    OS::StandardOutputAutoLock locker ;
    printf("ERROR: Can't create a log file name\n") ;
    return ;
  }

  log_file_path_ = log_path_.Append(file_name) ;
  log_file_.reset(
      new std::fstream(
          log_file_path_.value(), std::fstream::out | std::fstream::binary)) ;
  if (!log_file_ || !log_file_->is_open()) {
    log_path_.clear() ;
    log_file_path_.clear() ;
    log_file_.reset() ;
    OS::StandardOutputAutoLock locker ;
    printf(
        "ERROR: Can't open a log file - \'%s\'\n",
        log_file_path_.value().c_str()) ;
    return ;
  }

  vi::FilePath absolute_log_file_path = MakeAbsoluteFilePath(log_file_path_) ;
  if (!absolute_log_file_path.empty()) {
    log_file_path_ = absolute_log_file_path ;
    log_path_ = log_file_path_.DirName() ;
  }

  if (max_log_file_size_ <= 0) {
    max_log_file_size_ = kDefaultLogFileSize ;
  }
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
    bool file_flag = (file && file[0] != '\0') ;
    if (file_flag && error) {
      msg_suffix = vi::StringPrintf(
          message_file_error, error->name(), error->code(), file, line) ;
    } else if (file_flag) {
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

  if (log_file_) {
    (*log_file_) << msg_str ;
    log_file_size_ += msg_str.length() ;
    UpdateLogFile() ;
  }

  if (stdout_flag_) {
    OS::StandardOutputAutoLock locker ;
    std::cout << msg_str ;
  }

  if (stderr_flag_ && msg.log_level <= LogLevels::Warning) {
    OS::StandardOutputAutoLock locker ;
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

  // Flush all streams
  if (log_file_) {
    log_file_->flush() ;
  }

  if (stdout_flag_) {
    std::cout.flush() ;
  }

  if (stderr_flag_) {
    std::cerr.flush() ;
  }
}

void Logger::UpdateLogFile() {
  static const char footer_template[] =
      "============================== Log:%08X - Next file: "
      "%s ==============================\n" ;
  static const char header_template[] =
      "============================== Log:%08X - Previous file: "
      "%s ==============================\n" ;

  if (log_file_size_ <= max_log_file_size_ || log_level_ == LogLevels::None) {
    return ;
  }

  std::string file_name = CreateFreeFileName() ;
  if (file_name.empty()) {
    return ;
  }

  vi::FilePath log_file_path = log_path_.Append(file_name) ;
  std::unique_ptr<std::fstream> log_file(
      new std::fstream(
          log_file_path.value(), std::fstream::out | std::fstream::binary)) ;
  if (!log_file || !log_file->is_open()) {
    return ;
  }

  std::string msg = vi::StringPrintf(
      footer_template, log_id_, log_file_path.BaseName().value().c_str()) ;
  *log_file_ << msg ;
  log_file_->flush() ;
  msg = vi::StringPrintf(
      header_template, log_id_, log_file_path_.BaseName().value().c_str()) ;
  *log_file << msg ;

  log_file_.swap(log_file) ;
  log_file_path_ = std::move(log_file_path) ;
  log_file_size_ = 0 ;
}

void Logger::OnFatalError(
    const char* file, int line, const char* format, va_list args) {
  std::string msg = vi::StringPrintV(format, args) ;
  V8_LOG(
      LogLevels::Error, file + ::v8::vm::kLocalPathPrefixLength, line,
      "Fatal error occurred: \'%s\'", msg.c_str()) ;
}

void Logger::OnProcessAborted() {
  // Print a message with a stack trace
  v8::base::debug::StackTrace trace ;
  V8_LOG_ERR(
      errAborted, "The process has been aborted:\n%s",
      trace.ToString().c_str()) ;

  DeinitializeLog() ;
}

#endif  // defined(V8_VM_USE_LOG)

enum class DumpType {
  Context = 1,
  Heap = 2,
  HeapGraph = 3,
};

Error CreateDumpBySnapshotFromFile(
    DumpType type, const char* snapshot_path, v8::vm::FormattedJson formatted,
    const char* result_path) {
  vi::Data data(vi::Data::Type::Snapshot, snapshot_path) ;
  data.data = reinterpret_cast<const char*>(
    i::ReadBytes(snapshot_path, &data.size, true)) ;
  data.owner = true ;
  if (data.size == 0) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        errFileNotExists,
        "Snapshot file doesn't exist or is empty - \'%s\'", snapshot_path) ;
  }

  std::fstream fs(result_path, std::fstream::out | std::fstream::binary) ;
  if (fs.fail()) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        errFileNotOpened, "Can't open file \'%s\'", result_path) ;
  }

  StartupData snapshot = { data.data, data.size } ;
  std::unique_ptr<vi::WorkContext> context(vi::WorkContext::New(&snapshot)) ;
  Error result = errOk ;
  if (type == DumpType::Context) {
    result = CreateContextDump(*context, fs, formatted) ;
  } else if (type == DumpType::Heap) {
    result = CreateHeapDump(*context, fs) ;
  } else {
    result = CreateHeapGraphDump(*context, fs, formatted) ;
  }

  fs.close() ;
  V8_ERROR_RETURN_IF_FAILED(result) ;
  V8_LOG_INF(
      "Created a dump by the snapshot-file \'%s\' and saved result into \'%s\'",
      snapshot_path, result_path) ;
  return result ;
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
    V8_ERROR_ADD_MSG(result, "Can't create ScriptRunner") ;
    return result ;
  }

  result = runner->Run() ;
  V8_ERROR_RETURN_IF_FAILED(result) ;

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
      V8_LOG_WITH_ERROR(
          V8_ERROR_FAILED(*this) ? LogLevels::Error : LogLevels::Warning, *this,
          file, line, "%s", msg.c_str()) ;
    } else {
      V8_LOG(LogLevels::Info, file, line, "%s", msg.c_str()) ;
    }
  }

  return *this ;
}

Error& Error::CopyMessages(const Error& error, std::uint32_t offset) {
  // Get a back offset
  std::uint32_t back_offset =
      static_cast<std::uint32_t>(message_count()) - offset ;
  // Insert messages
  for (std::size_t i = 0, size = error.message_count(); i < size; ++i) {
    const Message& msg = error.message(i) ;
    AddMessage(msg.message, msg.file, msg.line, back_offset) ;
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
  V8_LOG_MSG("V8 version: %s", V8::GetVersion()) ;
  V8_LOG_MSG(
      "V8 VM version: %d.%d.%d", V8_VM_MAJOR_VERSION, V8_VM_MINOR_VERSION,
      V8_VM_BUILD_NUMBER) ;
#endif  // defined(V8_VM_USE_LOG)
}

void DeinitializeLog() {
#if defined(V8_VM_USE_LOG)
  Logger::DeinitializeLog() ;
#endif  // defined(V8_VM_USE_LOG)
}

#if defined(V8_VM_USE_LOG)

void PrintLogMessage(
  LogLevels log_level, const char* file, std::int32_t line,
  const char* msg, ...) {
  va_list ap ;
  va_start(ap, msg) ;
  Logger::PrintLogMessage(log_level, nullptr, file, line, msg, ap) ;
  va_end(ap) ;
}

void PrintLogMessage(
  LogLevels log_level, const Error& error, const char* file, std::int32_t line,
  const char* msg, ...) {
  va_list ap ;
  va_start(ap, msg) ;
  Logger::PrintLogMessage(log_level, &error, file, line, msg, ap) ;
  va_end(ap) ;
}

FunctionBodyLog::FunctionBodyLog(
    const char* function, const char* file, std::int32_t line, bool log_flag)
  : function_(function), file_(file), line_(line), log_flag_(log_flag),
    beginning_time_(new Time(Time::Now())) {
  if (log_flag_) {
    V8_LOG(
        LogLevels::Verbose, file_, line_, "\'%s\' - the beginning", function_) ;
  }
}

FunctionBodyLog::~FunctionBodyLog() {
  if (log_flag_) {
    TimeDelta delta = Time::Now() - *beginning_time_ ;
    V8_LOG(
        LogLevels::Verbose, file_, line_, "\'%s\' - the end "
        "(Execution time: %d.%06d seconds)", function_,
        static_cast<std::int32_t>(delta.InSeconds()),
        static_cast<std::int32_t>(
            delta.InMicroseconds() % Time::kMicrosecondsPerSecond)) ;
  }
}

#endif  // defined(V8_VM_USE_LOG)

void InitializeV8(const char* app_path, int* argc, char** argv) {
  V8HANDLE()->Initialize(app_path, argc, argv) ;
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
    V8_ERROR_ADD_MSG(res, "Can't compile the script") ;
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

Error CreateContextDumpBySnapshotFromFile(
    const char* snapshot_path, FormattedJson formatted,
    const char* result_path) {
  return CreateDumpBySnapshotFromFile(
      DumpType::Context, snapshot_path, formatted, result_path) ;
}

Error CreateHeapDumpBySnapshotFromFile(
    const char* snapshot_path, const char* result_path) {
  return CreateDumpBySnapshotFromFile(
      DumpType::Heap, snapshot_path, FormattedJson::False, result_path) ;
}

Error CreateHeapGraphDumpBySnapshotFromFile(
    const char* snapshot_path, FormattedJson formatted,
    const char* result_path) {
  return CreateDumpBySnapshotFromFile(
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
    V8_ERROR_ADD_MSG(result, "Can't create ScriptRunner") ;
    return result ;
  }

  result = runner->Run() ;
  V8_ERROR_RETURN_IF_FAILED(result) ;

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
    V8_ERROR_ADD_MSG(result, "Can't create ScriptRunner") ;
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
