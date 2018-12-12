// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_VM_H_
#define INCLUDE_V8_VM_H_

#include "v8.h"  // NOLINT(build/include)


namespace v8 {
namespace vm {

/** Errors */
#define V8_ERR_SUCCESSED(e) ((e) >= 0)
#define V8_ERR_FAILED(e) ((e) < 0)
#define V8_ERR_RETURN_IF_FAILED(e) if (V8_ERR_FAILED(e)) { return (e) ; }

typedef std::int32_t Error ;

const Error errOk = 0x0 ;

// Common warnings
const Error wrnIncompleteOperation = 0x0000001 ;
const Error wrnObjNotInit          = 0x0000002 ;

const Error errBase = 0xf0000000 ;

// Common errors
const Error errUnknown               = (errBase|0x0000001) ;
const Error errFailed                = (errBase|0x0000002) ;
const Error errAccessDenied          = (errBase|0x0000003) ;
const Error errObjNotInit            = (errBase|0x0000004) ;
const Error errTimeout               = (errBase|0x0000005) ;
const Error errInvalidArgument       = (errBase|0x0000006) ;
const Error errFileNotFound          = (errBase|0x0000007) ;
const Error errPathNotFound          = (errBase|0x0000008) ;
const Error errInsufficientResources = (errBase|0x0000009) ;
const Error errInvalidHandle         = (errBase|0x000000a) ;
const Error errOutOfMemory           = (errBase|0x000000b) ;
const Error errFileNoSpace           = (errBase|0x000000c) ;
const Error errFileExists            = (errBase|0x000000d) ;
const Error errFilePathTooLong       = (errBase|0x000000e) ;
const Error errNotImplemented        = (errBase|0x000000f) ;
const Error errAborted               = (errBase|0x0000010) ;
const Error errFileTooBig            = (errBase|0x0000011) ;
const Error errIncompleteOperation   = (errBase|0x0000012) ;

// Net errors
const Error errNetBase                 = (errBase|0x0001000) ;
const Error errNetIOPending            = (errNetBase|0x0000001) ;
const Error errNetInternetDisconnected = (errNetBase|0x0000002) ;
const Error errNetConnectionReset      = (errNetBase|0x0000003) ;
const Error errNetConnectionAborted    = (errNetBase|0x0000004) ;
const Error errNetConnectionRefused    = (errNetBase|0x0000005) ;
const Error errNetConnectionClosed     = (errNetBase|0x0000006) ;
const Error errNetSocketIsConnected    = (errNetBase|0x0000007) ;
const Error errNetAddressUnreachable   = (errNetBase|0x0000008) ;
const Error errNetAddressInvalid       = (errNetBase|0x0000009) ;
const Error errNetAddressInUse         = (errNetBase|0x000000a) ;
const Error errNetMsgTooBig            = (errNetBase|0x000000b) ;
const Error errNetSocketNotConnected   = (errNetBase|0x000000c) ;
const Error errNetInvalidPackage       = (errNetBase|0x000000d) ;
const Error errNetEntityTooLarge       = (errNetBase|0x000000e) ;

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
void V8_EXPORT CompileScript(
    const char* script, const char* script_origin,
    ScriptCompiler::CachedData& result) ;

/**
 * Compiles the script from file.
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
 * Runs the script.
 */
void V8_EXPORT RunScript(
    const char* script, const char* script_origin = nullptr,
    StartupData* snapshot_out = nullptr) ;

/**
 * Runs the script by using a js-script.
 */
void V8_EXPORT RunScriptByJSScriptFromFile(
    const char* js_path, const char* script_path,
    const char* snapshot_out_path = nullptr) ;

/**
 * Runs the script by using a previous compilation.
 */
void V8_EXPORT RunScriptByCompilationFromFile(
    const char* compilation_path, const char* script_path,
    const char* snapshot_out_path = nullptr) ;

/**
 * Runs the script by using a previous snapshot.
 */
void V8_EXPORT RunScriptBySnapshot(
    StartupData& snapshot, const char* script,
    const char* snapshot_origin = nullptr, const char* script_origin = nullptr,
    StartupData* snapshot_out = nullptr) ;

/**
 * Runs the script by using a previous snapshot from file.
 */
void V8_EXPORT RunScriptBySnapshotFromFile(
    const char* snapshot_path, const char* script_path,
    const char* snapshot_out_path = nullptr) ;

}  // namespace vm
}  // namespace v8

#endif  // INCLUDE_V8_VM_H_
