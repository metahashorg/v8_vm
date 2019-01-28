// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_VM_H_
#define INCLUDE_V8_VM_H_

#include "v8.h"  // NOLINT(build/include)
#include "v8-vm-error.h"
#include "v8-vm-log.h"


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
Error V8_EXPORT CompileScript(
    const char* script, const char* script_origin,
    ScriptCompiler::CachedData& result) V8_WARN_UNUSED_RESULT ;

/**
 * Compiles the script from file.
 */
Error V8_EXPORT CompileScriptFromFile(
    const char* script_path, const char* result_path) V8_WARN_UNUSED_RESULT ;

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
 * Runs the script.
 */
Error V8_EXPORT RunScript(
    const char* script, const char* script_origin = nullptr,
    StartupData* snapshot_out = nullptr) V8_WARN_UNUSED_RESULT ;

/**
 * Runs the script by using a js-script.
 */
Error V8_EXPORT RunScriptByJSScriptFromFile(
    const char* js_path, const char* script_path,
    const char* snapshot_out_path = nullptr) V8_WARN_UNUSED_RESULT ;

/**
 * Runs the script by using a previous compilation.
 */
Error V8_EXPORT RunScriptByCompilationFromFile(
    const char* compilation_path, const char* script_path,
    const char* snapshot_out_path = nullptr) V8_WARN_UNUSED_RESULT ;

/**
 * Runs the script by using a previous snapshot.
 */
Error V8_EXPORT RunScriptBySnapshot(
    StartupData& snapshot, const char* script,
    const char* snapshot_origin = nullptr, const char* script_origin = nullptr,
    StartupData* snapshot_out = nullptr) V8_WARN_UNUSED_RESULT ;

/**
 * Runs the script by using a previous snapshot from file.
 */
Error V8_EXPORT RunScriptBySnapshotFromFile(
    const char* snapshot_path, const char* script_path,
    const char* snapshot_out_path = nullptr) V8_WARN_UNUSED_RESULT ;

}  // namespace vm
}  // namespace v8

#endif  // INCLUDE_V8_VM_H_
