// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_COMMAND_LINE_H_
#define V8_VM_APPS_COMMAND_LINE_H_

#include <map>
#include <string>
#include <vector>

#include "include/v8config.h"

class CommandLine {
 public:
  using StringType = std::string ;
  using CharType = StringType::value_type ;
  using StringVector = std::vector<StringType> ;
  using SwitchMap = std::map<std::string, StringType> ;

  /** Constructs a new command line from an argument list. */
  CommandLine(int argc, const CharType* const* argv) ;

  /** Initializes from a argument list. */
  void InitFromArgv(int argc, const CharType* const* argv) ;
  void InitFromArgv(const StringVector& argv) ;

  /**
   * Get and set the program part of the command line string (the first item).
   */
  StringType GetProgram() const ;
  void SetProgram(const StringType& program) ;

  /** Returns a copy of all switches, along with their values. */
  const SwitchMap& GetSwitches() const { return switches_; }

  /**
   * Appends a switch [with optional value] to the command line.
   * Note: Switches will precede arguments regardless of appending order.
   */
  void AppendSwitchNative(const std::string& switch_string,
                          const StringType& value) ;

  /**
   * Returns true if the command line contains the switch.
   * Switch name must be lowercase.
   */
  bool HasSwitch(const char switch_constant[]) const ;

  /**
   * Returns a value associated with the switch.
   * If the switch has no value or isn't present,
   * this method returns a empty string.
   */
  StringType GetSwitchValueNative(const char switch_constant[]) const ;

  /** Returns the remaining arguments to the command. */
  StringVector GetArgs() const ;

  /** Returns a count of the remaining arguments to the command. */
  size_t GetArgCount() const ;

  /**
   * Appends an argument to the command line. Note that the argument is quoted
   * properly such that it is interpreted as one argument to the target command.
   */
  void AppendArgNative(const StringType& value) ;

#if defined(V8_OS_WIN)
  /**
   * By default this class will treat command-line arguments beginning with
   * slashes as switches on Windows, but not other platforms.
   * If this behavior is inappropriate for your application, you can call this
   * function BEFORE creating a command line object and the behavior will be
   * the same as Posix systems.
   */
  static void set_slash_is_not_a_switch() ;
#endif  // V8_OS_WIN

 private:
  /** Disallow default constructor. */
  CommandLine() ;

  /**
   * The argv array:
   * { program, [(--|-|/)switch[=value]]*, [--], [argument]*) }
   */
  StringVector argv_ ;

  /** Parsed switch keys and values. */
  SwitchMap switches_ ;

  /** The index after the program and switches, any arguments start here. */
  size_t begin_args_ ;
};

#endif  // V8_VM_APPS_COMMAND_LINE_H_
