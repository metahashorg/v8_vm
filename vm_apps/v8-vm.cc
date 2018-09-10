// Copyright 2018 the AdSniper project authors. All rights reserved.
//
//

#include "include/v8-vm.h"

enum class RequestType {
  Unknown = 0,
  Compile,
  Run,
  Dump,
};

RequestType GetRequestType(int argc, char* argv[]) {
  if (argc < 2) {
    return RequestType::Unknown ;
  }

  return RequestType::Compile ;
}

int main(int argc, char* argv[]) {
  RequestType request_type = GetRequestType(argc, argv) ;
  if (request_type == RequestType::Unknown) {
    printf("Specify what you want to do\n") ;
    return 1 ;
  }

  // Initialize V8
  v8::vm::InitializeV8(argv[0]) ;

  v8::vm::CompileScript(argv[1]) ;

  // Deinitialize V8
  v8::vm::DeinitializeV8() ;

  return 0 ;
}
