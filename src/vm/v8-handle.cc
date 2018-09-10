// Copyright 2018 the AdSniper project authors. All rights reserved.
//
//

#include "src/vm/v8-handle.h"

#include "include/libplatform/libplatform.h"

namespace v8 {
namespace vm {
namespace internal {

base::LazyInstance<V8Handle>::type
    V8Handle::instance_ = LAZY_INSTANCE_INITIALIZER ;

V8Handle::V8Handle() {}

void V8Handle::Initialize(const char* app_path) {
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
