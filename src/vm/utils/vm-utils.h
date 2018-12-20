// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_UTILS_VM_UTILS_H_
#define V8_VM_UTILS_VM_UTILS_H_

#include <cstring>

#include "src/handles.h"
#include "src/handles-inl.h"

// Helpful macros for a declaration of big enums
#define ENUM_COMMA ,
#define ENUM_LEFT_BRACKET (

#define ENUM_RECOMPOSE_ARGS(...) __VA_ARGS__

#define ENUM_INVOKE_ON_LIST_WITH_ARGS(fun, item_list, ...) \
  item_list(fun ENUM_LEFT_BRACKET ENUM_RECOMPOSE_ARGS(__VA_ARGS__) ENUM_COMMA)

#define ENUM_INVOKE_ON_LIST(fun, item_list) \
  item_list(fun ENUM_LEFT_BRACKET)

#define ENUM_BIT_NO_PARENT 0

#define ENUM_BIT_HELPER_ITEM(enum_name, name, ...) \
  enum_name ## _BitEnumHelper_ ## name

#define ENUM_BIT_HELPER_ITEM_DECLARATION(enum_name, name, ...) \
  ENUM_BIT_HELPER_ITEM(enum_name, name),

#define ENUM_BIT_TYPE_NAME(enum_name) enum_name ## _BitEnumHelper_type

#define ENUM_BIT_NUMBER_TO_ENUM_TYPE(enum_name, number) \
  (static_cast<ENUM_BIT_TYPE_NAME(enum_name)>(number))

#define ENUM_BIT_GET_ITEM_BIT(enum_name, item_name) \
  (ENUM_BIT_NUMBER_TO_ENUM_TYPE(enum_name, 1) << enum_name ## _BitEnumHelper:: \
      ENUM_BIT_HELPER_ITEM(enum_name, item_name))

#define ENUM_BIT_ITEM(enum_name, name, ...) \
  name = ENUM_BIT_GET_ITEM_BIT(enum_name, name),

#define ENUM_BIT_ITEM_WITH_PARENT(enum_name, name, parent, ...) \
  name = ENUM_BIT_GET_ITEM_BIT(enum_name, name) | parent,

#define ENUM_BIT_DECLARE_HELPER(enum_name, type, item_list) \
  typedef type ENUM_BIT_TYPE_NAME(enum_name) ; \
  enum enum_name ## _BitEnumHelper : type { \
    ENUM_INVOKE_ON_LIST_WITH_ARGS(ENUM_BIT_HELPER_ITEM_DECLARATION, \
        item_list, enum_name) \
    enum_name ## _BitEnumHelper_ ## Count, \
  }; \
  static_assert(enum_name ## _BitEnumHelper_ ## Count <= (sizeof(type) * 8), \
                "Too many items. To use a bigger type.") ;

#define ENUM_BIT_DECLARE_ITEMS(enum_name, item_list) \
  ENUM_INVOKE_ON_LIST_WITH_ARGS(ENUM_BIT_ITEM, item_list, enum_name)

#define ENUM_BIT_DECLARE_ITEMS_WITH_PARENT(enum_name, item_list) \
  ENUM_INVOKE_ON_LIST_WITH_ARGS(ENUM_BIT_ITEM_WITH_PARENT, item_list, enum_name)

#define ENUM_BIT_ALL_ITEM_COUNT(enum_name) \
  (enum_name ## _BitEnumHelper_ ## Count)

#define ENUM_BIT_ALL_ITEM_MASK(enum_name) \
  (~((~ ENUM_BIT_NUMBER_TO_ENUM_TYPE(enum_name, 0)) << \
      enum_name ## _BitEnumHelper_ ## Count))

#define ENUM_BIT_IS_PARENT(val, item) (!(val & (~item)))

#define ENUM_BIT_IS_CHILD(val, item) (!((~val) & item))

#define ENUM_BIT_GET_PARENT(enum_name, item_name) \
  static_cast<enum_name>((~ ENUM_BIT_GET_ITEM_BIT(enum_name, item_name)) & \
                         enum_name :: item_name)

#define ENUM_BIT_OPERATORS_DECLARATION(enum_name) \
  ENUM_BIT_TYPE_NAME(enum_name) operator ~(enum_name val) { \
    return (~static_cast<ENUM_BIT_TYPE_NAME(enum_name)>(val)) ; \
  } \
  ENUM_BIT_TYPE_NAME(enum_name) operator &(enum_name val1, enum_name val2) { \
    return (static_cast<ENUM_BIT_TYPE_NAME(enum_name)>(val1) & \
            static_cast<ENUM_BIT_TYPE_NAME(enum_name)>(val2)) ; \
  } \
  ENUM_BIT_TYPE_NAME(enum_name) operator &( \
      enum_name val1, ENUM_BIT_TYPE_NAME(enum_name) val2) { \
    return (static_cast<ENUM_BIT_TYPE_NAME(enum_name)>(val1) & val2) ; \
  } \
  ENUM_BIT_TYPE_NAME(enum_name) operator &( \
      ENUM_BIT_TYPE_NAME(enum_name) val1, enum_name val2) { \
    return (val2 & val1) ; \
  } \
  ENUM_BIT_TYPE_NAME(enum_name) operator |(enum_name val1, enum_name val2) { \
    return (static_cast<ENUM_BIT_TYPE_NAME(enum_name)>(val1) | \
            static_cast<ENUM_BIT_TYPE_NAME(enum_name)>(val2)) ; \
  } \
  ENUM_BIT_TYPE_NAME(enum_name) operator |( \
      enum_name val1, ENUM_BIT_TYPE_NAME(enum_name) val2) { \
    return (static_cast<ENUM_BIT_TYPE_NAME(enum_name)>(val1) | val2) ; \
  } \
  ENUM_BIT_TYPE_NAME(enum_name) operator |( \
      ENUM_BIT_TYPE_NAME(enum_name) val1, enum_name val2) { \
    return (val2 | val1) ; \
  }

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
  explicit TemporarilySetValue(T& variable, const T& value)
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

// Temporarily changes values of two variables
// NOTE: It has the same problem that TemporarilySetValue has
template<typename T>
class TemporarilyChangeValues {
 public:
  explicit TemporarilyChangeValues(T& var1, T& var2)
      : var1_(var1), var2_(var2) {
    std::swap(var1_, var2_) ;
  }

  ~TemporarilyChangeValues() {
    std::swap(var1_, var2_) ;
  }

 private:
  TemporarilyChangeValues() = delete ;

  T& var1_ ;
  T& var2_ ;
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
      memcpy(const_cast<char*>(data), data_pointer, size) ;
    }
  }

  void CopyData(const std::uint8_t* data_pointer, int data_size) {
    CopyData(reinterpret_cast<const char*>(data_pointer), data_size) ;
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

#endif  // V8_VM_UTILS_VM_UTILS_H_
