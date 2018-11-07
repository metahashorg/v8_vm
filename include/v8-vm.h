// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_VM_H_
#define INCLUDE_V8_VM_H_

#include "v8.h"  // NOLINT(build/include)


namespace v8 {
namespace vm {

/** Enum about how to write Json */
enum class FormattedJson {
  False = 0,
  True = 1
};

/** Initializes V8 for working with virtual machine. */
void V8_EXPORT InitializeV8(const char* app_path) ;

/** Deinitializes V8. */
void V8_EXPORT DeinitializeV8() ;

/**
 * Compiles the script.
 */
void V8_EXPORT CompileScriptFromFile(
    const char* script_path, const char* result_path) ;

/**
 * Creates a context dump by a snapshot
 */
void V8_EXPORT CreateContextDumpBySnapshotFromFile(
    const char* snapshot_path, FormattedJson formatted,
    const char* result_path) ;

/**
 * Creates a heap dump by a snapshot
 */
void V8_EXPORT CreateHeapDumpBySnapshotFromFile(
    const char* snapshot_path, const char* result_path) ;

/**
 * Creates a heap graph dump by a snapshot
 */
void V8_EXPORT CreateHeapGraphDumpBySnapshotFromFile(
    const char* snapshot_path, FormattedJson formatted,
    const char* result_path) ;

/**
 * Runs the script by using a js-script.
 */
void V8_EXPORT RunScriptByJSScriptFromFile(
    const char* js_path, const char* script_path,
    const char* out_snapshot_path = nullptr) ;

/**
 * Runs the script by using a previous compilation.
 */
void V8_EXPORT RunScriptByCompilationFromFile(
    const char* compilation_path, const char* script_path,
    const char* out_snapshot_path = nullptr) ;

/**
 * Runs the script by using a previous snapshot.
 */
void V8_EXPORT RunScriptBySnapshotFromFile(
    const char* snapshot_path, const char* script_path,
    const char* out_snapshot_path = nullptr) ;

}  // namespace vm
}  // namespace v8

#endif  // INCLUDE_V8_VM_H_
