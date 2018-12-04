// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This provides a wrapper around system calls which may be interrupted by a
// signal and return EINTR. See man 7 signal.
// To prevent long-lasting loops (which would likely be a bug, such as a signal
// that should be masked) to go unnoticed, there is a limit after which the
// caller will nonetheless see an EINTR in Debug builds.
//
// On Windows and Fuchsia, this wrapper macro does nothing because there are no
// signals.
//
// Don't wrap close calls in HANDLE_EINTR. Use IGNORE_EINTR if the return
// value of close is significant. See http://crbug.com/269623.

#ifndef V8_VM_APPS_HTTP_SERVER_EINTR_WRAPPER_H_
#define V8_VM_APPS_HTTP_SERVER_EINTR_WRAPPER_H_

#include "src/base/build_config.h"

#if defined(V8_OS_POSIX) && !defined(V8_OS_FUCHSIA)

#include <errno.h>

#if defined(NDEBUG)

#define HANDLE_EINTR(x) ({ \
  decltype(x) eintr_wrapper_result ; \
  do { \
    eintr_wrapper_result = (x) ; \
  } while (eintr_wrapper_result == -1 && errno == EINTR) ; \
  eintr_wrapper_result ; \
})

#else

#define HANDLE_EINTR(x) ({ \
  int eintr_wrapper_counter = 0 ; \
  decltype(x) eintr_wrapper_result ; \
  do { \
    eintr_wrapper_result = (x) ; \
  } while (eintr_wrapper_result == -1 && errno == EINTR && \
           eintr_wrapper_counter++ < 100) ; \
  eintr_wrapper_result ; \
})

#endif  // NDEBUG

#define IGNORE_EINTR(x) ({ \
  decltype(x) eintr_wrapper_result ; \
  do { \
    eintr_wrapper_result = (x) ; \
    if (eintr_wrapper_result == -1 && errno == EINTR) { \
      eintr_wrapper_result = 0 ; \
    } \
  } while (0) ; \
  eintr_wrapper_result ; \
})

#else  // !V8_OS_POSIX || V8_OS_FUCHSIA

#define HANDLE_EINTR(x) (x)
#define IGNORE_EINTR(x) (x)

#endif  // !V8_OS_POSIX || V8_OS_FUCHSIA

#endif  // V8_VM_APPS_HTTP_SERVER_EINTR_WRAPPER_H_
