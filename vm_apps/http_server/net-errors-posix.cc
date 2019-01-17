// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/net-errors.h"

#include <errno.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>

Error MapSystemError(SystemErrorCode os_error) {
  // There are numerous posix error codes, but these are the ones we thus far
  // find interesting.
  switch (os_error) {
    case EAGAIN:
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
       return errNetIOPending ;
    case EACCES:
       return errAccessDenied ;
    case ENETDOWN:
       return errNetInternetDisconnected ;
    case ETIMEDOUT:
       return errTimeout ;
    case ECONNRESET:
    case ENETRESET:  // Related to keep-alive.
    case EPIPE:
       return errNetConnectionReset ;
    case ECONNABORTED:
       return errNetConnectionAborted ;
    case ECONNREFUSED:
       return errNetConnectionRefused ;
    case EHOSTUNREACH:
    case EHOSTDOWN:
    case ENETUNREACH:
    case EAFNOSUPPORT:
       return errNetAddressUnreachable;
    case EADDRNOTAVAIL:
       return errNetAddressInvalid ;
    case EMSGSIZE:
       return errNetMsgTooBig ;
    case ENOTCONN:
       return errNetSocketNotConnected ;
    case EISCONN:
       return errNetSocketIsConnected ;
    case EINVAL:
       return errInvalidArgument ;
    case EADDRINUSE:
       return errNetAddressInUse ;
    case E2BIG:  // Argument list too long.
       return errInvalidArgument ;
    case EBADF:  // Bad file descriptor.
       return errInvalidHandle ;
    case EBUSY:  // Device or resource busy.
       return errInsufficientResources ;
    case ECANCELED:  // Operation canceled.
       return errAborted ;
    case EDEADLK:  // Resource deadlock avoided.
       return errInsufficientResources ;
    case EDQUOT:  // Disk quota exceeded.
       return errFileNoSpace ;
    case EEXIST:  // File exists.
       return errFileExists ;
    case EFAULT:  // Bad address.
       return errInvalidArgument ;
    case EFBIG:  // File too large.
       return errFileTooBig ;
    case EISDIR:  // Operation not allowed for a directory.
       return errAccessDenied ;
    case ENAMETOOLONG:  // Filename too long.
       return errFilePathTooLong ;
    case ENFILE:  // Too many open files in system.
       return errInsufficientResources ;
    case ENOBUFS:  // No buffer space available.
       return errOutOfMemory ;
    case ENODEV:  // No such device.
       return errInvalidArgument ;
    case ENOENT:  // No such file or directory.
       return errFileNotFound ;
    case ENOLCK:  // No locks available.
       return errInsufficientResources ;
    case ENOMEM:  // Not enough space.
       return errOutOfMemory ;
    case ENOSPC:  // No space left on device.
       return errFileNoSpace ;
    case ENOSYS:  // Function not implemented.
       return errNotImplemented ;
    case ENOTDIR:  // Not a directory.
       return errFileNotFound ;
    case ENOTSUP:  // Operation not supported.
       return errNotImplemented ;
    case EPERM:  // Operation not permitted.
       return errAccessDenied ;
    case EROFS:  // Read-only file system.
       return errAccessDenied ;
    case ETXTBSY:  // Text file busy.
       return errAccessDenied ;
    case EUSERS:  // Too many users.
       return errInsufficientResources ;
    case EMFILE:  // Too many open files.
       return errInsufficientResources ;

    case 0:
       return errOk;
    default:
      printf("ERROR: Unknown error - 0x%08X", (std::uint32_t)os_error) ;
       return errFailed ;
  }
}
