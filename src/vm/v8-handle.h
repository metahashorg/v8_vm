// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_V8_HANDLE_H_
#define V8_VM_V8_HANDLE_H_

#include "include/v8.h"
#include "src/base/lazy-instance.h"
#include "src/base/macros.h"

#define V8HANDLE() v8::vm::internal::V8Handle::instance_.Pointer()

namespace v8 {
namespace vm {
namespace internal {

class V8Handle {
 public:
  void Initialize(const char* app_path, int* argc, char** argv) ;
  void Deinitialize() ;

  static base::LazyInstance<V8Handle>::type instance_ ;

 private:
  V8Handle() ;

  std::unique_ptr<Platform> platform_ ;

  DISALLOW_COPY_AND_ASSIGN(V8Handle) ;

  friend struct base::DefaultConstructTrait<V8Handle> ;
};

}  // namespace internal
}  // namespace vm
}  // namespace v8

#endif  // V8_VM_V8_HANDLE_H_
