// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_VM_UTILS_H_
#define V8_VM_VM_UTILS_H_

#include <cstring>

#include "src/handles.h"
#include "src/handles-inl.h"

namespace v8 {
namespace vm {
namespace internal {

// Converts a utf8-string into a internal V8 string
static inline Local<String> Utf8ToStr(Isolate* isolate, const char* str) {
  return String::NewFromUtf8(isolate, str, NewStringType::kNormal)
             .ToLocalChecked() ;
}

// Converts a v8::Value (e.g. v8::String) into a utf8-string
template <class T>
static inline std::string ValueToUtf8(Isolate* isolate, T value) {
  Local<Value> value_as_value = T::template Cast<Value>(value) ;
  if (value_as_value.IsEmpty()) {
    return "" ;
  }

  Local<String> value_as_string = value_as_value->ToString() ;
  v8::String::Utf8Value str(isolate, value_as_string) ;
  return *str ? *str : "" ;
}

template <class T>
static inline std::string ValueToUtf8(Local<Context> context, T value) {
  if (context.IsEmpty()) {
    printf("ERROR: Context is null\n") ;
    return "" ;
}

  return ValueToUtf8(context->GetIsolate(), value) ;
}

// Temporarily sets a value into a variable
// NOTE: Need to think of multithreading. Now here is a potential problem.
// E.g {var:true}->{thrd1 var:false,cache:true}->{thrd2 var:false,cache:false}->
//         {thrd1 var:true}->{thrd2 var:false}
template<typename T>
class TemporarilySetValue {
 public:
  explicit TemporarilySetValue(T& variable, T value)
      : variable_(variable), cache_(value) {
    std::swap(variable_, cache_) ;
  }

  ~TemporarilySetValue() {
    std::swap(variable_, cache_) ;
  }

 private:
  TemporarilySetValue() = delete ;

  T& variable_ ;
  T cache_ ;
};

// We need some structure of a file content and so on for using during calls.
struct Data {
  enum class Type {
    Unknown = 0,
    None,
    JSScript,
    Compilation,
    Snapshot,
  };

  Data(Type data_type = Type::Unknown,
       const char* data_origin = nullptr,
       const char* data_pointer = nullptr,
       int data_size = 0,
       bool data_owner = false)
    : type(data_type),
      origin(data_origin ? data_origin : ""),
      data(data_pointer),
      size(data_size),
      owner(data_owner) {
    if (type == Type::JSScript && data && !size) {
      size = static_cast<int>(strlen(data)) + 1 ;
    }
  }

  ~Data() {
    if (owner && data) {
      delete [] data ;
    }
  }

  void CopyData(const char* data_pointer, int data_size) {
    if (owner && size) {
      delete [] data ;
    }

    data = nullptr ;
    size = data_size ;
    owner = true ;
    if (size) {
      data = new char[size] ;
      memcpy((void*)data, data_pointer, size) ;
    }
  }

  Data& operator =(const Data& other) {
    type = other.type ;
    origin = other.origin ;
    data = other.data ;
    size = other.size ;
    owner = false ;
    return *this ;
  }

  Type type ;
  std::string origin ;
  const char* data ;
  int size ;
  bool owner ;

  Data(const Data&) = delete ;
};

}  // namespace internal
}  // namespace vm
}  // namespace v8

#endif  // V8_VM_VM_UTILS_H_
