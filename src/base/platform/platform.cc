// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/platform.h"

#include <set>

#include "src/base/platform/mutex.h"

namespace v8 {
namespace base {

namespace {

// Mutex of standard output
LazyRecursiveMutex g_standard_output_mutex = LAZY_RECURSIVE_MUTEX_INITIALIZER ;

// Abort callbacks
std::set<OS::AbortCallback> g_abort_callbacks = {} ;

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

void OS::AddAbortCallback(OS::AbortCallback callback) {
  g_abort_callbacks.insert(callback) ;
}

void OS::RemoveAbortCallback(OS::AbortCallback callback) {
  g_abort_callbacks.erase(callback) ;
}

void OS::CallAbortCallbacks() {
  std::set<OS::AbortCallback> abort_callbacks = g_abort_callbacks ;
  for (auto it : abort_callbacks) {
    (*it)() ;
  }
}

}  // base
}  // v8
