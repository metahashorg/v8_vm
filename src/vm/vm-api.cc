// Copyright 2018 the AdSniper project authors. All rights reserved.
//
//

#include "include/v8-vm.h"
#include "src/vm/v8-handle.h"

namespace vi = v8::vm::internal ;

namespace v8 {
namespace vm {

void InitializeV8(const char* app_path) {
  vi::V8Handle::instance_.Pointer()->Initialize(app_path) ;
}

void DeinitializeV8() {
  vi::V8Handle::instance_.Pointer()->Deinitialize() ;
}

}  // namespace vm
}  // namespace v8
