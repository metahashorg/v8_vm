// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_VM_H_
#define INCLUDE_V8_VM_H_

#include "v8.h"  // NOLINT(build/include)


namespace v8 {
namespace vm {

  /** Initializes V8 for working with virtual machine. */
  void V8_EXPORT InitializeV8(const char* app_path) ;

  /** Deinitializes V8. */
  void V8_EXPORT DeinitializeV8() ;

  /**
   * Compiles the script.
   * Result will be saved into a file with the same name and
   * extension is .cmpl.
   */
  void V8_EXPORT CompileScript(const char* script_path) ;

  /**
   * Runs the script by using a previous compilation.
   */
  void V8_EXPORT RunScriptByCompilation(
      const char* compilation_path, const char* script_path) ;

}  // namespace vm
}  // namespace v8

#endif  // INCLUDE_V8_VM_H_
