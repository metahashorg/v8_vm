// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/platform.h"

#include "src/base/platform/mutex.h"

namespace v8 {
namespace base {

namespace {

LazyRecursiveMutex g_standard_output_mutex = LAZY_RECURSIVE_MUTEX_INITIALIZER ;

}  // namespace

OS::StandardOutputAutoLock::StandardOutputAutoLock()
  : StandardOutputAutoLock(stdout) {}

OS::StandardOutputAutoLock::StandardOutputAutoLock(FILE* stream)
  : stream_(stream) {
  if (stream_ == stdout || stream_ == stderr) {
    g_standard_output_mutex.Pointer()->Lock() ;
  }
}

OS::StandardOutputAutoLock::~StandardOutputAutoLock() {
  if (stream_ == stdout || stream_ == stderr) {
    g_standard_output_mutex.Pointer()->Unlock() ;
  }
}

}  // base
}  // v8
