// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/command-line.h"

#include <algorithm>

#include "vm_apps/app-utils.h"

namespace {

const CommandLine::CharType kSwitchTerminator[] = "--" ;
const CommandLine::CharType kSwitchValueSeparator[] = "=" ;

#if defined(V8_OS_WIN)
// By putting slash last, we can control whether it is treaded as a switch
// value by changing the value of switch_prefix_count to be one less than
// the array size.
const CommandLine::CharType* const kSwitchPrefixes[] = {"--", "-", "/"} ;
#elif defined(V8_OS_POSIX)
// Unixes don't use slash as a switch.
const CommandLine::CharType* const kSwitchPrefixes[] = {"--", "-"} ;
#endif

size_t switch_prefix_count = arraysize(kSwitchPrefixes) ;

size_t GetSwitchPrefixLength(const CommandLine::StringType& string) {
  for (size_t i = 0; i < switch_prefix_count; ++i) {
    CommandLine::StringType prefix(kSwitchPrefixes[i]) ;
    if (string.compare(0, prefix.length(), prefix) == 0)
      return prefix.length() ;
  }
  return 0 ;
}

// Fills in |switch_string| and |switch_value| if |string| is a switch.
// This will preserve the input switch prefix in the output |switch_string|.
bool IsSwitch(const CommandLine::StringType& string,
              CommandLine::StringType* switch_string,
              CommandLine::StringType* switch_value) {
  switch_string->clear() ;
  switch_value->clear() ;
  size_t prefix_length = GetSwitchPrefixLength(string) ;
  if (prefix_length == 0 || prefix_length == string.length())
    return false ;

  const size_t equals_position = string.find(kSwitchValueSeparator) ;
  *switch_string = string.substr(0, equals_position) ;
  if (equals_position != CommandLine::StringType::npos)
    *switch_value = string.substr(equals_position + 1) ;
  return true ;
}

// Append switches and arguments, keeping switches before arguments.
void AppendSwitchesAndArguments(CommandLine* command_line,
                                const CommandLine::StringVector& argv) {
  bool parse_switches = true ;
  for (size_t i = 1; i < argv.size(); ++i) {
    CommandLine::StringType arg = argv[i] ;
    TrimWhitespaceASCII(arg, TRIM_ALL, &arg) ;

    CommandLine::StringType switch_string ;
    CommandLine::StringType switch_value ;
    parse_switches &= (arg != kSwitchTerminator) ;
    if (parse_switches && IsSwitch(arg, &switch_string, &switch_value)) {
      command_line->AppendSwitchNative(switch_string, switch_value) ;
    } else {
      command_line->AppendArgNative(arg) ;
    }
  }
}

}  // namespace

CommandLine::CommandLine(int argc, const CharType* const* argv)
    : argv_(1),
      begin_args_(1) {
  InitFromArgv(argc, argv) ;
}

void CommandLine::InitFromArgv(int argc, const CharType* const* argv) {
  StringVector new_argv;
  for (int i = 0; i < argc; ++i)
    new_argv.push_back(argv[i]) ;

  InitFromArgv(new_argv) ;
}

void CommandLine::InitFromArgv(const StringVector& argv) {
  argv_ = StringVector(1) ;
  switches_.clear() ;
  begin_args_ = 1 ;
  SetProgram(argv.empty() ? StringType() : StringType(argv[0])) ;
  AppendSwitchesAndArguments(this, argv) ;
}

CommandLine::StringType CommandLine::GetProgram() const {
  return argv_[0] ;
}

void CommandLine::SetProgram(const StringType& program) {
  TrimWhitespaceASCII(program, TRIM_ALL, &argv_[0]) ;
}

void CommandLine::AppendSwitchNative(const std::string& switch_string,
                                     const StringType& value) {
  const std::string& switch_key = switch_string ;
  StringType combined_switch_string(switch_key) ;

  size_t prefix_length = GetSwitchPrefixLength(combined_switch_string) ;
  auto insertion =
      switches_.insert(make_pair(switch_key.substr(prefix_length), value)) ;
  if (!insertion.second)
    insertion.first->second = value ;

  // Preserve existing switch prefixes in |argv_|; only append one if necessary.
  if (prefix_length == 0)
    combined_switch_string = kSwitchPrefixes[0] + combined_switch_string ;

  if (!value.empty())
    combined_switch_string += kSwitchValueSeparator + value ;

  // Append the switch and update the switches/arguments divider |begin_args_|.
  argv_.insert(argv_.begin() + begin_args_++, combined_switch_string) ;
}

bool CommandLine::HasSwitch(const char switch_constant[]) const {
  return switches_.find(switch_constant) != switches_.end() ;
}

CommandLine::StringType CommandLine::GetSwitchValueNative(
    const char switch_constant[]) const {
  auto result = switches_.find(switch_constant) ;
  return result == switches_.end() ? StringType() : result->second ;
}

CommandLine::StringVector CommandLine::GetArgs() const {
  // Gather all arguments after the last switch (may include kSwitchTerminator).
  StringVector args(argv_.begin() + begin_args_, argv_.end()) ;
  // Erase only the first kSwitchTerminator (maybe "--" is a legitimate page?)
  StringVector::iterator switch_terminator =
      std::find(args.begin(), args.end(), kSwitchTerminator) ;
  if (switch_terminator != args.end())
    args.erase(switch_terminator) ;

  return args;
}

size_t CommandLine::GetArgCount() const {
  // Count all arguments after the last switch (may include kSwitchTerminator).
  size_t result = argv_.size() - begin_args_ ;
  // Decrease result for only the first kSwitchTerminator
  StringVector::const_iterator switch_terminator =
      std::find(argv_.begin() + begin_args_, argv_.end(), kSwitchTerminator) ;
  if (switch_terminator != argv_.end())
    --result ;
  return result ;
}

void CommandLine::AppendArgNative(const StringType& value) {
  argv_.push_back(value) ;
}

#if defined(V8_OS_WIN)
// static
void CommandLine::set_slash_is_not_a_switch() {
  // The last switch prefix should be slash, so adjust the size to skip it.
  switch_prefix_count = arraysize(kSwitchPrefixes) - 1;
}
#endif  // V8_OS_WIN
