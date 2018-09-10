// Copyright 2018 the AdSniper project authors. All rights reserved.
//
//

#ifndef V8_VM_V8_HANDLE_H_
#define V8_VM_V8_HANDLE_H_

#include "include/v8.h"
#include "src/base/lazy-instance.h"
#include "src/base/macros.h"

namespace v8 {
namespace vm {
namespace internal {

class V8Handle {
 public:
  void Initialize(const char* app_path) ;
  void Deinitialize() ;

  const Isolate::CreateParams& create_params() const { return create_params_ ; }

  static base::LazyInstance<V8Handle>::type instance_ ;

 private:
  V8Handle() ;

  std::unique_ptr<Platform> platform_ ;
  Isolate::CreateParams create_params_ ;

  DISALLOW_COPY_AND_ASSIGN(V8Handle) ;

  friend struct base::DefaultConstructTrait<V8Handle> ;
};

}  // namespace internal
}  // namespace vm
}  // namespace v8

#endif  // V8_VM_V8_HANDLE_H_
