// Copyright 2018 the AdSniper project authors. All rights reserved.
//
//

#include "include/v8-vm.h"

int main(int argc, char* argv[]) {
  // Initialize V8
  v8::vm::InitializeV8(argv[0]) ;

  // Deinitialize V8
  v8::vm::DeinitializeV8() ;

  return 0 ;
}
