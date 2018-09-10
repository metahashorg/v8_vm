// Copyright 2018 the AdSniper project authors. All rights reserved.
//
//

#ifndef INCLUDE_V8_VM_H_
#define INCLUDE_V8_VM_H_

#include "v8.h"  // NOLINT(build/include)


namespace v8 {
namespace vm {

  /**
   * Initialize V8 for working with virtual machine
   */
  void V8_EXPORT InitializeV8(const char* app_path) ;

  /**
   * Deinitialize V8
   */
  void V8_EXPORT DeinitializeV8() ;

}  // namespace vm
}  // namespace v8

#endif  // INCLUDE_V8_VM_H_
