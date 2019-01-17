// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_VM_ERROR_H_
#define INCLUDE_V8_VM_ERROR_H_

#include <string>

#include "v8.h"  // NOLINT(build/include)


namespace v8 {
namespace vm {

// This file name in project
// NOTE: Change definition if you move or rename it
#define V8_ERROR_FILE_NAME "include/v8-vm-error.h"

// Define a file name macro in the project
constexpr std::int32_t string_length_with_delta(
    const char* str, std::int32_t delta = 0) {
  return str[0] ? string_length_with_delta(str + 1, delta + 1) : delta ;
}

constexpr std::int32_t kLocalPathPrefixLength = string_length_with_delta(
    __FILE__, -string_length_with_delta(V8_ERROR_FILE_NAME)) ;

#define V8_PROJECT_FILE_NAME (__FILE__ + ::v8::vm::kLocalPathPrefixLength)

// Define a type flag macro. |type| may be either WarningTypes or ErrorTypes
#define V8_ERROR_TYPE_FLAG(type, name) \
  (type :: name != type ::None ? (0x1000 << \
      (type :: name != type ::None ? \
          static_cast<ErrorCodeType>(type :: name) : 0)) : \
      0)

// Helpful macros for creation warning and error name
#define V8_ERROR_ERROR_NAME_true(type, name) err ## type ## name
#define V8_ERROR_ERROR_NAME_false(type, name) err ## name
#define V8_ERROR_WARNING_NAME_true(type, name) wrn ## type ## name
#define V8_ERROR_WARNING_NAME_false(type, name) wrn ## name
#define V8_ERROR_WARNING_NAME_TO_STR_true(type, name) "wrn" #type #name
#define V8_ERROR_WARNING_NAME_TO_STR_false(type, name) "wrn" #name
#define V8_ERROR_ERROR_NAME_TO_STR_true(type, name) "err" #type #name
#define V8_ERROR_ERROR_NAME_TO_STR_false(type, name) "err" #name
#define V8_ERROR_ERROR_NAME(type, prefix_flag, name) \
  V8_ERROR_ERROR_NAME_ ## prefix_flag (type, name)
#define V8_ERROR_WARNING_NAME(type, prefix_flag, name) \
  V8_ERROR_WARNING_NAME_ ## prefix_flag (type, name)

// Error code type
typedef std::int32_t ErrorCodeType ;

// List warning types
enum class WarningTypes : ErrorCodeType {
  None = -1,
  #define V8_WARNING_TYPE(type_name) type_name,
  #include "include/v8-vm-error-list.h"
  #undef V8_WARNING_TYPE
  Count,
};

// List error types
enum class ErrorTypes : ErrorCodeType {
  None = -1,
  #define V8_ERROR_TYPE(type_name) type_name,
  #include "include/v8-vm-error-list.h"
  #undef V8_ERROR_TYPE
  Count,
};

// List warning and error codes
enum class ErrorCodes : ErrorCodeType {
  // Flag of a error code
  errFlag = static_cast<ErrorCodeType>(0x80000000),

  // Add warning types
  #define V8_WARNING_TYPE(type_name) \
    wrn ## type_name ## Flag = V8_ERROR_TYPE_FLAG(WarningTypes, type_name),
  #include "include/v8-vm-error-list.h"
  #undef V8_WARNING_TYPE

  // Add error types
  #define V8_ERROR_TYPE(type_name) \
    err ## type_name ## Flag = V8_ERROR_TYPE_FLAG(ErrorTypes, type_name),
  #include "include/v8-vm-error-list.h"
  #undef V8_ERROR_TYPE

  // Add warnings
  #define V8_WARNING(type_name, prefix_flag, name, id, ...) \
    V8_ERROR_WARNING_NAME(type_name, prefix_flag, name) = \
      (V8_ERROR_TYPE_FLAG(WarningTypes, type_name) | id),
  #include "include/v8-vm-error-list.h"
  #undef V8_WARNING

  // Add errors
  #define V8_ERROR(type_name, prefix_flag, name, id, ...) \
    V8_ERROR_ERROR_NAME(type_name, prefix_flag, name) = \
      static_cast<ErrorCodeType>( \
      ((ErrorTypes:: type_name != ErrorTypes::None ? \
        (errFlag | V8_ERROR_TYPE_FLAG(ErrorTypes, type_name)) : 0) | id)),
  #include "include/v8-vm-error-list.h"
  #undef V8_ERROR
};

// ErrorCodes operators
static inline ErrorCodeType operator &(ErrorCodes val1, ErrorCodes val2) {
  return (static_cast<ErrorCodeType>(val1) & static_cast<ErrorCodeType>(val2)) ;
}

// Functions for working with ErrorCodes
static inline const char* GetErrorDescription(ErrorCodes error) {
  #define V8_ERROR_MSG_PREFIX_true(type) #type " "
  #define V8_ERROR_MSG_PREFIX_false(type)

  switch (error) {
  // Warnings
  #define V8_WARNING(type_name, prefix_flag, name, id, description) \
    case ErrorCodes:: V8_ERROR_WARNING_NAME(type_name, prefix_flag, name): \
      return V8_ERROR_MSG_PREFIX_ ## prefix_flag(type_name) \
             "WARNING: " description ;
  #include "include/v8-vm-error-list.h"
  #undef V8_WARNING
  // Errors
  #define V8_ERROR(type_name, prefix_flag, name, id, description) \
    case ErrorCodes:: V8_ERROR_ERROR_NAME(type_name, prefix_flag, name): \
      return V8_ERROR_MSG_PREFIX_ ## prefix_flag(type_name) \
             "ERROR: " description ;
  #include "include/v8-vm-error-list.h"
  #undef V8_ERROR

    default:
      break ;
  };

  return "Undefined error code" ;
}

static inline const char* GetErrorName(ErrorCodes error) {
  switch (error) {
  // Warnings
  #define V8_WARNING(type_name, prefix_flag, name, ...) \
    case ErrorCodes:: V8_ERROR_WARNING_NAME(type_name, prefix_flag, name): \
      return V8_ERROR_WARNING_NAME_TO_STR_ ## prefix_flag(type_name, name) ;
  #include "include/v8-vm-error-list.h"
  #undef V8_WARNING
  // Errors
  #define V8_ERROR(type_name, prefix_flag, name, ...) \
    case ErrorCodes:: V8_ERROR_ERROR_NAME(type_name, prefix_flag, name): \
      return V8_ERROR_ERROR_NAME_TO_STR_ ## prefix_flag(type_name, name) ;
  #include "include/v8-vm-error-list.h"
  #undef V8_ERROR

    default:
      break ;
  };

  return "UndefinedErrorCode" ;
}

// Helpful macros for errors
#define V8_ERROR_SUCCESS(e) (!(e.code() & ::v8::vm::ErrorCodes::errFlag))
#define V8_ERROR_FAILED(e) (e.code() & ::v8::vm::ErrorCodes::errFlag)
#define V8_ERROR_RETURN_IF_FAILED(e) \
  if (V8_ERROR_FAILED(e)) { \
    return (e) ; \
  }

// Main error descriptor
class V8_EXPORT Error {
 public:
  struct Message {
    std::string message ;
    const char* file = "" ;
    std::uint32_t line = 0 ;
  };

  Error(ErrorCodes code, const char* file, std::uint32_t line)
    : code_(code), file_(file), line_(line) {}

  operator ErrorCodeType() { return static_cast<ErrorCodeType>(code_) ; }
  operator ErrorCodes() { return code_ ; }
  bool operator ==(const Error& error) { return code_ == error.code() ; }
  bool operator !=(const Error& error) { return code_ != error.code() ; }

  ErrorCodes code() const { return code_ ; }
  const char* description() const { return GetErrorDescription(code_) ; }
  const char* name() const { return GetErrorName(code_) ; }
  const char* file() const { return file_ ; }
  std::uint32_t line() const { return line_ ; }

 private:
  ErrorCodes code_ = ErrorCodes::errOk ;
  const char* file_ = "" ;
  std::uint32_t line_ = 0 ;
};

// Await implementation of language feature:
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4519.pdf
//
// After it will be implemented to add a constructor:
// Error(
//     ErrorCodes error_code,
//     const std::experimental::source_location& location =
//         std::experimental::source_location::current()) {...}
//
// In the meanwhile we use a macro implementation of it
#define V8_ERROR_CODE_DEFINITION(code) \
  Error(::v8::vm::ErrorCodes:: code, V8_PROJECT_FILE_NAME, __LINE__)

// Code of success
#define errOk V8_ERROR_CODE_DEFINITION(errOk)
// Common warnings
#define wrnIncompleteOperation V8_ERROR_CODE_DEFINITION(wrnIncompleteOperation)
#define wrnObjNotInit V8_ERROR_CODE_DEFINITION(wrnObjNotInit)
// Common errors
#define errUnknown V8_ERROR_CODE_DEFINITION(errUnknown)
#define errFailed V8_ERROR_CODE_DEFINITION(errFailed)
#define errAccessDenied V8_ERROR_CODE_DEFINITION(errAccessDenied)
#define errObjNotInit V8_ERROR_CODE_DEFINITION(errObjNotInit)
#define errTimeout V8_ERROR_CODE_DEFINITION(errTimeout)
#define errInvalidArgument V8_ERROR_CODE_DEFINITION(errInvalidArgument)
#define errFileNotFound V8_ERROR_CODE_DEFINITION(errFileNotFound)
#define errPathNotFound V8_ERROR_CODE_DEFINITION(errPathNotFound)
#define errInsufficientResources \
  V8_ERROR_CODE_DEFINITION(errInsufficientResources)
#define errInvalidHandle V8_ERROR_CODE_DEFINITION(errInvalidHandle)
#define errOutOfMemory V8_ERROR_CODE_DEFINITION(errOutOfMemory)
#define errFileNoSpace V8_ERROR_CODE_DEFINITION(errFileNoSpace)
#define errFileExists V8_ERROR_CODE_DEFINITION(errFileExists)
#define errFilePathTooLong V8_ERROR_CODE_DEFINITION(errFilePathTooLong)
#define errNotImplemented V8_ERROR_CODE_DEFINITION(errNotImplemented)
#define errAborted V8_ERROR_CODE_DEFINITION(errAborted)
#define errFileTooBig V8_ERROR_CODE_DEFINITION(errFileTooBig)
#define errIncompleteOperation V8_ERROR_CODE_DEFINITION(errIncompleteOperation)
#define errUnsupportedType V8_ERROR_CODE_DEFINITION(errUnsupportedType)
#define errNotEnoughData V8_ERROR_CODE_DEFINITION(errNotEnoughData)
#define errFileNotExists V8_ERROR_CODE_DEFINITION(errFileNotExists)
#define errFileEmpty V8_ERROR_CODE_DEFINITION(errFileEmpty)
// JS errors
#define errJSUnknown V8_ERROR_CODE_DEFINITION(errJSUnknown)
#define errJSException V8_ERROR_CODE_DEFINITION(errJSException)
#define errJSCacheRejected V8_ERROR_CODE_DEFINITION(errJSCacheRejected)
// Json errors
#define errJsonInvalidEscape V8_ERROR_CODE_DEFINITION(errJsonInvalidEscape)
#define errJsonSyntaxError V8_ERROR_CODE_DEFINITION(errJsonSyntaxError)
#define errJsonUnexpectedToken V8_ERROR_CODE_DEFINITION(errJsonUnexpectedToken)
#define errJsonTrailingComma V8_ERROR_CODE_DEFINITION(errJsonTrailingComma)
#define errJsonTooMuchNesting V8_ERROR_CODE_DEFINITION(errJsonTooMuchNesting)
#define errJsonUnexpectedDataAfterRoot \
  V8_ERROR_CODE_DEFINITION(errJsonUnexpectedDataAfterRoot)
#define errJsonUnsupportedEncoding \
  V8_ERROR_CODE_DEFINITION(errJsonUnsupportedEncoding)
#define errJsonUnquotedDictionaryKey \
  V8_ERROR_CODE_DEFINITION(errJsonUnquotedDictionaryKey)
#define errJsonInappropriateType \
  V8_ERROR_CODE_DEFINITION(errJsonInappropriateType)
// Net errors
#define errNetIOPending V8_ERROR_CODE_DEFINITION(errNetIOPending)
#define errNetInternetDisconnected \
  V8_ERROR_CODE_DEFINITION(errNetInternetDisconnected)
#define errNetConnectionReset V8_ERROR_CODE_DEFINITION(errNetConnectionReset)
#define errNetConnectionAborted \
  V8_ERROR_CODE_DEFINITION(errNetConnectionAborted)
#define errNetConnectionRefused \
  V8_ERROR_CODE_DEFINITION(errNetConnectionRefused)
#define errNetConnectionClosed V8_ERROR_CODE_DEFINITION(errNetConnectionClosed)
#define errNetSocketIsConnected \
  V8_ERROR_CODE_DEFINITION(errNetSocketIsConnected)
#define errNetAddressUnreachable \
  V8_ERROR_CODE_DEFINITION(errNetAddressUnreachable)
#define errNetAddressInvalid V8_ERROR_CODE_DEFINITION(errNetAddressInvalid)
#define errNetAddressInUse V8_ERROR_CODE_DEFINITION(errNetAddressInUse)
#define errNetMsgTooBig V8_ERROR_CODE_DEFINITION(errNetMsgTooBig)
#define errNetSocketNotConnected \
  V8_ERROR_CODE_DEFINITION(errNetSocketNotConnected)
#define errNetInvalidPackage V8_ERROR_CODE_DEFINITION(errNetInvalidPackage)
#define errNetEntityTooLarge V8_ERROR_CODE_DEFINITION(errNetEntityTooLarge)

}  // namespace vm
}  // namespace v8

#endif  // INCLUDE_V8_VM_ERROR_H_
