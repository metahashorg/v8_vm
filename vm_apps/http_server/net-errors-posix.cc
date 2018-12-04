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

vv::Error MapSystemError(SystemErrorCode os_error) {
  // There are numerous posix error codes, but these are the ones we thus far
  // find interesting.
  switch (os_error) {
    case EAGAIN:
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
       return vv::errNetIOPending ;
    case EACCES:
       return vv::errAccessDenied ;
    case ENETDOWN:
       return vv::errNetInternetDisconnected ;
    case ETIMEDOUT:
       return vv::errTimeout ;
    case ECONNRESET:
    case ENETRESET:  // Related to keep-alive.
    case EPIPE:
       return vv::errNetConnectionReset ;
    case ECONNABORTED:
       return vv::errNetConnectionAborted ;
    case ECONNREFUSED:
       return vv::errNetConnectionRefused ;
    case EHOSTUNREACH:
    case EHOSTDOWN:
    case ENETUNREACH:
    case EAFNOSUPPORT:
       return vv::errNetAddressUnreachable;
    case EADDRNOTAVAIL:
       return vv::errNetAddressInvalid ;
    case EMSGSIZE:
       return vv::errNetMsgTooBig ;
    case ENOTCONN:
       return vv::errNetSocketNotConnected ;
    case EISCONN:
       return vv::errNetSocketIsConnected ;
    case EINVAL:
       return vv::errInvalidArgument ;
    case EADDRINUSE:
       return vv::errNetAddressInUse ;
    case E2BIG:  // Argument list too long.
       return vv::errInvalidArgument ;
    case EBADF:  // Bad file descriptor.
       return vv::errInvalidHandle ;
    case EBUSY:  // Device or resource busy.
       return vv::errInsufficientResources ;
    case ECANCELED:  // Operation canceled.
       return vv::errAborted ;
    case EDEADLK:  // Resource deadlock avoided.
       return vv::errInsufficientResources ;
    case EDQUOT:  // Disk quota exceeded.
       return vv::errFileNoSpace ;
    case EEXIST:  // File exists.
       return vv::errFileExists ;
    case EFAULT:  // Bad address.
       return vv::errInvalidArgument ;
    case EFBIG:  // File too large.
       return vv::errFileTooBig ;
    case EISDIR:  // Operation not allowed for a directory.
       return vv::errAccessDenied ;
    case ENAMETOOLONG:  // Filename too long.
       return vv::errFilePathTooLong ;
    case ENFILE:  // Too many open files in system.
       return vv::errInsufficientResources ;
    case ENOBUFS:  // No buffer space available.
       return vv::errOutOfMemory ;
    case ENODEV:  // No such device.
       return vv::errInvalidArgument ;
    case ENOENT:  // No such file or directory.
       return vv::errFileNotFound ;
    case ENOLCK:  // No locks available.
       return vv::errInsufficientResources ;
    case ENOMEM:  // Not enough space.
       return vv::errOutOfMemory ;
    case ENOSPC:  // No space left on device.
       return vv::errFileNoSpace ;
    case ENOSYS:  // Function not implemented.
       return vv::errNotImplemented ;
    case ENOTDIR:  // Not a directory.
       return vv::errFileNotFound ;
    case ENOTSUP:  // Operation not supported.
       return vv::errNotImplemented ;
    case EPERM:  // Operation not permitted.
       return vv::errAccessDenied ;
    case EROFS:  // Read-only file system.
       return vv::errAccessDenied ;
    case ETXTBSY:  // Text file busy.
       return vv::errAccessDenied ;
    case EUSERS:  // Too many users.
       return vv::errInsufficientResources ;
    case EMFILE:  // Too many open files.
       return vv::errInsufficientResources ;

    case 0:
       return vv::errOk;
    default:
      printf("ERROR: Unknown error - 0x%08X", (std::uint32_t)os_error) ;
       return vv::errFailed ;
  }
}
