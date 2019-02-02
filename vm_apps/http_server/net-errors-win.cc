// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/net-errors.h"

#include <winsock2.h>

// Map winsock and system errors to V8 errors.
Error MapSystemError(SystemErrorCode os_error) {
  // There are numerous Winsock error codes, but these are the ones we thus far
  // find interesting.
  switch (os_error) {
    case WSAEWOULDBLOCK:
    case WSA_IO_PENDING:
       return errNetIOPending ;
    case WSAEACCES:
       return errAccessDenied ;
    case WSAENETDOWN:
       return errNetInternetDisconnected ;
    case WSAETIMEDOUT:
    case WSA_WAIT_TIMEOUT:
       return errTimeout ;
    case WSAECONNRESET:
    case WSAENETRESET:  // Related to keep-alive
       return errNetConnectionReset ;
    case WSAECONNABORTED:
       return errNetConnectionAborted ;
    case WSAECONNREFUSED:
       return errNetConnectionRefused ;
    case WSA_IO_INCOMPLETE:
    case WSAEDISCON:
       return errNetConnectionClosed ;
    case WSAEISCONN:
       return errNetSocketIsConnected ;
    case WSAEHOSTUNREACH:
    case WSAENETUNREACH:
       return errNetAddressUnreachable ;
    case WSAEADDRNOTAVAIL:
       return errNetAddressInvalid ;
    case WSAEMSGSIZE:
       return errNetMsgTooBig ;
    case WSAENOTCONN:
       return errNetSocketNotConnected ;
    case WSAEAFNOSUPPORT:
       return errNetAddressUnreachable ;
    case WSAEINVAL:
       return errInvalidArgument ;
    case WSAEADDRINUSE:
       return errNetAddressInUse ;

    // System errors.
    case ERROR_FILE_NOT_FOUND:  // The system cannot find the file specified.
       return errFileNotFound ;
    case ERROR_PATH_NOT_FOUND:  // The system cannot find the path specified.
       return errPathNotFound ;
    case ERROR_TOO_MANY_OPEN_FILES:  // The system cannot open the file.
       return errInsufficientResources ;
    case ERROR_ACCESS_DENIED:  // Access is denied.
       return errAccessDenied ;
    case ERROR_INVALID_HANDLE:  // The handle is invalid.
       return errInvalidHandle ;
    case ERROR_NOT_ENOUGH_MEMORY:   // Not enough storage is available to
       return errOutOfMemory ;  // process this command.
    case ERROR_OUTOFMEMORY:         // Not enough storage is available
       return errOutOfMemory ;  // to complete this operation.
    case ERROR_WRITE_PROTECT:  // The media is write protected.
       return errAccessDenied ;
    case ERROR_SHARING_VIOLATION:    // Cannot access the file because it is
       return errAccessDenied ;  // being used by another process.
    case ERROR_LOCK_VIOLATION:       // The process cannot access the file because
       return errAccessDenied ;  // another process has locked the file.
    case ERROR_HANDLE_EOF:  // Reached the end of the file.
       return errFailed ;
    case ERROR_HANDLE_DISK_FULL:  // The disk is full.
       return errFileNoSpace ;
    case ERROR_FILE_EXISTS:  // The file exists.
       return errFileExists ;
    case ERROR_INVALID_PARAMETER:  // The parameter is incorrect.
       return errInvalidArgument ;
    case ERROR_BUFFER_OVERFLOW:  // The file name is too long.
       return errFilePathTooLong ;
    case ERROR_DISK_FULL:  // There is not enough space on the disk.
       return errFileNoSpace ;
    case ERROR_CALL_NOT_IMPLEMENTED:   // This function is not supported on
       return errNotImplemented ;  // this system.
    case ERROR_INVALID_NAME:            // The filename, directory name,
       return errInvalidArgument ;  // or volume label syntax is incorrect.
    case ERROR_DIR_NOT_EMPTY:  // The directory is not empty.
       return errFailed ;
    case ERROR_BUSY:  // The requested resource is in use.
       return errAccessDenied ;
    case ERROR_ALREADY_EXISTS:      // Cannot create a file when that file
       return errFileExists ;   // already exists.
    case ERROR_FILENAME_EXCED_RANGE:  // The filename or extension is too long.
       return errFilePathTooLong ;
    case ERROR_FILE_TOO_LARGE:      // The file size exceeds the limit allowed
       return errFileNoSpace ;  // and cannot be saved.
    case ERROR_IO_DEVICE:            // The request could not be performed
       return errAccessDenied ;  // because of an I/O device error.
    case ERROR_POSSIBLE_DEADLOCK:    // A potential deadlock condition has
       return errAccessDenied ;  // been detected.
    case ERROR_BAD_DEVICE:  // The specified device name is invalid.
       return errInvalidArgument ;

    case ERROR_SUCCESS:
       return errOk;
    default:
      return V8_ERROR_CREATE_WITH_MSG_SP(
          errFailed, "Unknown error - %d",
          static_cast<std::uint32_t>(os_error)) ;
  }
}
