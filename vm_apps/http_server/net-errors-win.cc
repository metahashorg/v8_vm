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
vv::Error MapSystemError(SystemErrorCode os_error) {
  // There are numerous Winsock error codes, but these are the ones we thus far
  // find interesting.
  switch (os_error) {
    case WSAEWOULDBLOCK:
    case WSA_IO_PENDING:
       return vv::errNetIOPending ;
    case WSAEACCES:
       return vv::errAccessDenied ;
    case WSAENETDOWN:
       return vv::errNetInternetDisconnected ;
    case WSAETIMEDOUT:
    case WSA_WAIT_TIMEOUT:
       return vv::errTimeout ;
    case WSAECONNRESET:
    case WSAENETRESET:  // Related to keep-alive
       return vv::errNetConnectionReset ;
    case WSAECONNABORTED:
       return vv::errNetConnectionAborted ;
    case WSAECONNREFUSED:
       return vv::errNetConnectionRefused ;
    case WSA_IO_INCOMPLETE:
    case WSAEDISCON:
       return vv::errNetConnectionClosed ;
    case WSAEISCONN:
       return vv::errNetSocketIsConnected ;
    case WSAEHOSTUNREACH:
    case WSAENETUNREACH:
       return vv::errNetAddressUnreachable ;
    case WSAEADDRNOTAVAIL:
       return vv::errNetAddressInvalid ;
    case WSAEMSGSIZE:
       return vv::errNetMsgTooBig ;
    case WSAENOTCONN:
       return vv::errNetSocketNotConnected ;
    case WSAEAFNOSUPPORT:
       return vv::errNetAddressUnreachable ;
    case WSAEINVAL:
       return vv::errInvalidArgument ;
    case WSAEADDRINUSE:
       return vv::errNetAddressInUse ;

    // System errors.
    case ERROR_FILE_NOT_FOUND:  // The system cannot find the file specified.
       return vv::errFileNotFound ;
    case ERROR_PATH_NOT_FOUND:  // The system cannot find the path specified.
       return vv::errPathNotFound ;
    case ERROR_TOO_MANY_OPEN_FILES:  // The system cannot open the file.
       return vv::errInsufficientResources ;
    case ERROR_ACCESS_DENIED:  // Access is denied.
       return vv::errAccessDenied ;
    case ERROR_INVALID_HANDLE:  // The handle is invalid.
       return vv::errInvalidHandle ;
    case ERROR_NOT_ENOUGH_MEMORY:   // Not enough storage is available to
       return vv::errOutOfMemory ;  // process this command.
    case ERROR_OUTOFMEMORY:         // Not enough storage is available
       return vv::errOutOfMemory ;  // to complete this operation.
    case ERROR_WRITE_PROTECT:  // The media is write protected.
       return vv::errAccessDenied ;
    case ERROR_SHARING_VIOLATION:    // Cannot access the file because it is
       return vv::errAccessDenied ;  // being used by another process.
    case ERROR_LOCK_VIOLATION:       // The process cannot access the file because
       return vv::errAccessDenied ;  // another process has locked the file.
    case ERROR_HANDLE_EOF:  // Reached the end of the file.
       return vv::errFailed ;
    case ERROR_HANDLE_DISK_FULL:  // The disk is full.
       return vv::errFileNoSpace ;
    case ERROR_FILE_EXISTS:  // The file exists.
       return vv::errFileExists ;
    case ERROR_INVALID_PARAMETER:  // The parameter is incorrect.
       return vv::errInvalidArgument ;
    case ERROR_BUFFER_OVERFLOW:  // The file name is too long.
       return vv::errFilePathTooLong ;
    case ERROR_DISK_FULL:  // There is not enough space on the disk.
       return vv::errFileNoSpace ;
    case ERROR_CALL_NOT_IMPLEMENTED:   // This function is not supported on
       return vv::errNotImplemented ;  // this system.
    case ERROR_INVALID_NAME:            // The filename, directory name,
       return vv::errInvalidArgument ;  // or volume label syntax is incorrect.
    case ERROR_DIR_NOT_EMPTY:  // The directory is not empty.
       return vv::errFailed ;
    case ERROR_BUSY:  // The requested resource is in use.
       return vv::errAccessDenied ;
    case ERROR_ALREADY_EXISTS:      // Cannot create a file when that file
       return vv::errFileExists ;   // already exists.
    case ERROR_FILENAME_EXCED_RANGE:  // The filename or extension is too long.
       return vv::errFilePathTooLong ;
    case ERROR_FILE_TOO_LARGE:      // The file size exceeds the limit allowed
       return vv::errFileNoSpace ;  // and cannot be saved.
    case ERROR_IO_DEVICE:            // The request could not be performed
       return vv::errAccessDenied ;  // because of an I/O device error.
    case ERROR_POSSIBLE_DEADLOCK:    // A potential deadlock condition has
       return vv::errAccessDenied ;  // been detected.
    case ERROR_BAD_DEVICE:  // The specified device name is invalid.
       return vv::errInvalidArgument ;

    case ERROR_SUCCESS:
       return vv::errOk;
    default:
      printf("ERROR: Unknown error - 0x%08X", (std::uint32_t)os_error) ;
       return vv::errFailed ;
  }
}
