// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/vm/v8-handle.h"

#include "src/flags.h"
#include "src/globals.h"
#include "include/libplatform/libplatform.h"

namespace v8 {
namespace vm {
namespace internal {

namespace {

void SetGlobalV8Flags() {
  // TODO: Look at other flags

  // Compilation flags
  i::FLAG_lazy = false ;
  i::FLAG_log_code = true ;

#ifdef DEBUG
  // Use for a verbose deserialization
  i::FLAG_profile_deserialization = true ;
#endif  // DEBUG
}

}  // namespace

base::LazyInstance<V8Handle>::type
    V8Handle::instance_ = LAZY_INSTANCE_INITIALIZER ;

V8Handle::V8Handle() {}

void V8Handle::Initialize(const char* app_path, int* argc, char** argv) {
  // Set global V8 flags here because multithreading and
  // V8 calculates a hash of flags during initialization
  // Flags can be changed by command line
  SetGlobalV8Flags() ;
  if (argc) {
    i::FlagList::SetFlagsFromCommandLine(argc, argv, false) ;
  }

  V8::InitializeICUDefaultLocation(app_path) ;
  V8::InitializeExternalStartupData(app_path) ;
  platform_ = platform::NewDefaultPlatform() ;
  V8::InitializePlatform(platform_.get()) ;
  V8::Initialize() ;
  create_params_.array_buffer_allocator =
      ArrayBuffer::Allocator::NewDefaultAllocator() ;
}

void V8Handle::Deinitialize() {
  V8::Dispose() ;
  V8::ShutdownPlatform() ;
  delete create_params_.array_buffer_allocator ;
  platform_ = nullptr ;
}

}  // namespace internal
}  // namespace vm
}  // namespace v8
