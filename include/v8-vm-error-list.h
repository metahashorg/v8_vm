// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(V8_ERROR) && !defined(V8_ERROR_TYPE) && \
    !defined(V8_WARNING_TYPE) && !defined(V8_WARNING)
#error "None of macros (V8_ERROR, V8_ERROR_TYPE, V8_WARNING_TYPE, V8_WARNING) \
is defined. You should do it before including this file."
#endif  // !defined(V8_ERROR) && !defined(V8_ERROR_TYPE) &&
        // !defined(V8_WARNING_TYPE) && !defined(V8_WARNING)

// NOTE: Add new warning and error types at the end. Don't change id of codes
//       because it is passed to other services.

// Instructions
// ------------
//
// V8_WARNING_TYPE(type_name)
//   |type_name| - warning type name. Use for a warning definition
//
// Examples:
// V8_WARNING_TYPE(Common)
// V8_WARNING_TYPE(JS)
//
//
// V8_WARNING(type_name, prefix_flag, name, id, description)
//   |type_name|   - name of a warning type (Use |None| for independent warning.
//                   In that case |id| equates a full warning code)
//   |prefix_flag| - using or not a type name as prefix in a warning name
//                   (true|false)
//   |name|        - warning name
//   |id|          - warning id (std::int32_t)
//   |description| - warning description (c-string)
//
// Examples:
// V8_WARNING(Common, false, ObjNotInit, 0x2, "Object isn't initialized")
// V8_WARNING(JS, true, ArgNotFound, 0x4, "JS: Argumend isn't found")
// V8_WARNING(None, false, MyWarning, 0x1234, "My warning description")
//
//
// V8_ERROR_TYPE(type_name)
//   |type_name| - error type name. Use for a error definition
//
// Examples:
// V8_ERROR_TYPE(Common)
// V8_ERROR_TYPE(Net)
//
//
// V8_ERROR(type_name, prefix_flag, name, id, description)
//   |type_name|   - name of a error type (Use |None| for independent error.
//                   In that case |id| equates a full error code)
//   |prefix_flag| - using or not a type name as prefix in a error name
//                   (true|false)
//   |name|        - error name
//   |id|          - error id (std::int32_t)
//   |description| - error description (c-string)
//
// Examples:
// V8_ERROR(Common, false, Unknown, 0x2, "Unknown error ocurred")
// V8_WARNING(Net, true, InvalidPackage, 0x4, "Net: Package is invalid")
// V8_WARNING(None, false, MyError, 0x80001234, "My error description")
//

// Code of success
#if defined(V8_ERROR)
V8_ERROR(None, false, Ok, 0x0, "Success")
#endif  // defined(V8_ERROR)

// Common warnings
#if defined(V8_WARNING_TYPE)
V8_WARNING_TYPE(Common)
#endif  // defined(V8_WARNING_TYPE)

#if defined(V8_WARNING)
V8_WARNING(Common, false, IncompleteOperation, 0x1,
    "The operation was incomplete")
V8_WARNING(Common, false, ObjNotInit, 0x2, "The object was not initialized")
#endif  // defined(V8_WARNING)

// Common errors
#if defined(V8_ERROR_TYPE)
V8_ERROR_TYPE(Common)
#endif  // defined(V8_ERROR_TYPE)

#if defined(V8_ERROR)
V8_ERROR(Common, false, Unknown, 0x1, "Unknown error occurred")
V8_ERROR(Common, false, Failed, 0x2, "The operation failed")
V8_ERROR(Common, false, AccessDenied, 0x3, "Access denied")
V8_ERROR(Common, false, ObjNotInit, 0x4, "The object was not initialized")
V8_ERROR(Common, false, Timeout, 0x5, "Timeout occurred")
V8_ERROR(Common, false, InvalidArgument, 0x6, "Argument is invalid")
V8_ERROR(Common, false, FileNotFound, 0x7, "The file was not found")
V8_ERROR(Common, false, PathNotFound, 0x8, "The path was not found")
V8_ERROR(Common, false, InsufficientResources, 0x9, "Lack of free resources")
V8_ERROR(Common, false, InvalidHandle, 0xa, "The handle is invalid")
V8_ERROR(Common, false, OutOfMemory, 0xb,
    "No additional memory can be allocated")
V8_ERROR(Common, false, FileNoSpace, 0xc,
    "There is no space left on the device")
V8_ERROR(Common, false, FileExists, 0xd, "The file exists")
V8_ERROR(Common, false, FilePathTooLong, 0xe, "The file path is too long")
V8_ERROR(Common, false, NotImplemented, 0xf,
    "The functional was not implemented")
V8_ERROR(Common, false, Aborted, 0x10, "The operation was aborted")
V8_ERROR(Common, false, FileTooBig, 0x11, "The file is too big")
V8_ERROR(Common, false, IncompleteOperation, 0x12,
    "The operation was incomplete")
V8_ERROR(Common, false, UnsupportedType, 0x13, "The type is not supported")
V8_ERROR(Common, false, NotEnoughData, 0x14,
    "Not enough data to complete operation")
V8_ERROR(Common, false, FileNotExists, 0x15, "The file does not exist")
V8_ERROR(Common, false, FileEmpty, 0x16, "The file is empty")
#endif  // defined(V8_ERROR)

// JS errors
#if defined(V8_ERROR_TYPE)
V8_ERROR_TYPE(JS)
#endif  // defined(V8_ERROR_TYPE)

#if defined(V8_ERROR)
V8_ERROR(JS, true, Unknown, 0x1, "Unknown error ocurred")
V8_ERROR(JS, true, Exception, 0x2, "Exception ocurred")
V8_ERROR(JS, true, CacheRejected, 0x3, "Cache was rejected")
#endif  // defined(V8_ERROR)

// Json errors
#if defined(V8_ERROR_TYPE)
V8_ERROR_TYPE(Json)
#endif  // defined(V8_ERROR_TYPE)

#if defined(V8_ERROR)
V8_ERROR(Json, true, InvalidEscape, 0x1, "Escaped symbol could not be parsed")
V8_ERROR(Json, true, SyntaxError, 0x2, "The json has a syntax error")
V8_ERROR(Json, true, UnexpectedToken, 0x3,
    "During parsing a unexpected token was encountered")
V8_ERROR(Json, true, TrailingComma, 0x4,
    "The last item of object has a comma after itself")
V8_ERROR(Json, true, TooMuchNesting, 0x5, "The json has too deep nesting")
V8_ERROR(Json, true, UnexpectedDataAfterRoot, 0x6,
    "The json has unexpected data after root item")
V8_ERROR(Json, true, UnsupportedEncoding, 0x7,
    "String has unsupported encoding")
V8_ERROR(Json, true, UnquotedDictionaryKey, 0x8,
    "The dictionary key has to be quoted")
V8_ERROR(Json, true, InappropriateType, 0x9,
    "Inappropriate type was encountered")
#endif  // defined(V8_ERROR)

// Net errors
#if defined(V8_ERROR_TYPE)
V8_ERROR_TYPE(Net)
#endif  // defined(V8_ERROR_TYPE)

#if defined(V8_ERROR)
V8_ERROR(Net, true, IOPending, 0x1,
    "The operation is started but the result is not ready yet")
V8_ERROR(Net, true, InternetDisconnected, 0x2,
    "The Internet connection has been lost")
V8_ERROR(Net, true, ConnectionReset, 0x3,
    "A connection was reset (corresponding to a TCP RST)")
V8_ERROR(Net, true, ConnectionAborted, 0x4,
    "A connection timed out as a result of not receiving an ACK for data sent")
V8_ERROR(Net, true, ConnectionRefused, 0x5, "A connection attempt was refused")
V8_ERROR(Net, true, ConnectionClosed, 0x6,
    "A connection was closed (corresponding to a TCP FIN)")
V8_ERROR(Net, true, SocketIsConnected, 0x7, "The socket is already connected")
V8_ERROR(Net, true, AddressUnreachable, 0x8, "The IP address is unreachable")
V8_ERROR(Net, true, AddressInvalid, 0x9,
    "The IP address or port number is invalid")
V8_ERROR(Net, true, AddressInUse, 0xa,
    "Attempting to bind an address that is already in use")
V8_ERROR(Net, true, MsgTooBig, 0xb,
    "The message was too large for the transport")
V8_ERROR(Net, true, SocketNotConnected, 0xc, "The socket is not connected")
V8_ERROR(Net, true, InvalidPackage, 0xd, "The net package is invalid")
V8_ERROR(Net, true, EntityTooLarge, 0xe,
    "Net entity is too large for processing")
#endif  // defined(V8_ERROR)
