// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_UTILS_VM_UTILS_H_
#define V8_VM_UTILS_VM_UTILS_H_

#include <cstring>

#include "src/vm/utils/string-printf.h"

// Macros for writing an exception into an error
#define V8_ERROR_CREATE_BY_TRY_CATCH(context, error, try_catch) \
  Local<v8::Message> tc_msg = try_catch.Message() ; \
  if (try_catch.HasCaught() && !tc_msg.IsEmpty()) { \
    error = V8_ERROR_CREATE_WITH_MSG_SP( \
        errJSException, \
        "Origin:\'%s\' Line:%i Column:%i Source line:\'%s\' - %s", \
        ValueToUtf8( \
            context, tc_msg->GetScriptOrigin().ResourceName()).c_str(), \
        tc_msg->GetLineNumber(context).FromMaybe(0), \
        tc_msg->GetStartColumn(context).FromMaybe(-1) + 1, \
        ValueToUtf8( \
            context, \
            tc_msg->GetSourceLine(context).FromMaybe(Local<String>())) \
            .c_str(), \
        ValueToUtf8(context, tc_msg->Get()).c_str()) ; \
  } else { \
    error = try_catch.HasCaught() ? errJSException : errJSUnknown ; \
    printf("JS ERROR: Unknown error occurred (File:%s Line:%d)", \
           V8_PROJECT_FILE_NAME, __LINE__) ; \
  }

// Helpful macros for a declaration of big enums
#define V8_ENUM_COMMA ,
#define V8_ENUM_LEFT_BRACKET (

#define V8_ENUM_RECOMPOSE_ARGS(...) __VA_ARGS__

#define V8_ENUM_INVOKE_ON_LIST_WITH_ARGS(fun, item_list, ...) \
  item_list(fun V8_ENUM_LEFT_BRACKET V8_ENUM_RECOMPOSE_ARGS(__VA_ARGS__) \
            V8_ENUM_COMMA)

#define V8_ENUM_INVOKE_ON_LIST(fun, item_list) \
  item_list(fun V8_ENUM_LEFT_BRACKET)

#define V8_ENUM_BIT_NO_PARENT 0

#define V8_ENUM_BIT_HELPER_ITEM(enum_name, name, ...) \
  enum_name ## _BitEnumHelper_ ## name

#define V8_ENUM_BIT_HELPER_ITEM_DECLARATION(enum_name, name, ...) \
  V8_ENUM_BIT_HELPER_ITEM(enum_name, name),

#define V8_ENUM_BIT_TYPE_NAME(enum_name) enum_name ## _BitEnumHelper_type

#define V8_ENUM_BIT_NUMBER_TO_ENUM_TYPE(enum_name, number) \
  (static_cast<V8_ENUM_BIT_TYPE_NAME(enum_name)>(number))

#define V8_ENUM_BIT_GET_ITEM_BIT(enum_name, item_name) \
  (V8_ENUM_BIT_NUMBER_TO_ENUM_TYPE(enum_name, 1) << \
    enum_name ## _BitEnumHelper:: V8_ENUM_BIT_HELPER_ITEM(enum_name, item_name))

#define V8_ENUM_BIT_ITEM(enum_name, name, ...) \
  name = V8_ENUM_BIT_GET_ITEM_BIT(enum_name, name),

#define V8_ENUM_BIT_ITEM_WITH_PARENT(enum_name, name, parent, ...) \
  name = V8_ENUM_BIT_GET_ITEM_BIT(enum_name, name) | parent,

#define V8_ENUM_BIT_DECLARE_HELPER(enum_name, type, item_list) \
  typedef type V8_ENUM_BIT_TYPE_NAME(enum_name) ; \
  enum enum_name ## _BitEnumHelper : type { \
    V8_ENUM_INVOKE_ON_LIST_WITH_ARGS(V8_ENUM_BIT_HELPER_ITEM_DECLARATION, \
        item_list, enum_name) \
    enum_name ## _BitEnumHelper_ ## Count, \
  }; \
  static_assert(enum_name ## _BitEnumHelper_ ## Count <= (sizeof(type) * 8), \
                "Too many items. To use a bigger type.") ;

#define V8_ENUM_BIT_DECLARE_ITEMS(enum_name, item_list) \
  V8_ENUM_INVOKE_ON_LIST_WITH_ARGS(V8_ENUM_BIT_ITEM, item_list, enum_name)

#define V8_ENUM_BIT_DECLARE_ITEMS_WITH_PARENT(enum_name, item_list) \
  V8_ENUM_INVOKE_ON_LIST_WITH_ARGS( \
    V8_ENUM_BIT_ITEM_WITH_PARENT, item_list, enum_name)

#define V8_ENUM_BIT_ALL_ITEM_COUNT(enum_name) \
  (enum_name ## _BitEnumHelper_ ## Count)

#define V8_ENUM_BIT_ALL_ITEM_MASK(enum_name) \
  (~((~ V8_ENUM_BIT_NUMBER_TO_ENUM_TYPE(enum_name, 0)) << \
      enum_name ## _BitEnumHelper_ ## Count))

#define V8_ENUM_BIT_IS_PARENT(val, item) (!(val & (~item)))

#define V8_ENUM_BIT_IS_CHILD(val, item) (!((~val) & item))

#define V8_ENUM_BIT_GET_PARENT(enum_name, item_name) \
  static_cast<enum_name>((~ V8_ENUM_BIT_GET_ITEM_BIT(enum_name, item_name)) & \
                         enum_name :: item_name)

#define V8_ENUM_BIT_OPERATORS_DECLARATION(enum_name) \
  V8_ENUM_BIT_TYPE_NAME(enum_name) operator ~(enum_name val) { \
    return (~static_cast<V8_ENUM_BIT_TYPE_NAME(enum_name)>(val)) ; \
  } \
  V8_ENUM_BIT_TYPE_NAME(enum_name) operator &( \
      enum_name val1, enum_name val2) { \
    return (static_cast<V8_ENUM_BIT_TYPE_NAME(enum_name)>(val1) & \
            static_cast<V8_ENUM_BIT_TYPE_NAME(enum_name)>(val2)) ; \
  } \
  V8_ENUM_BIT_TYPE_NAME(enum_name) operator &( \
      enum_name val1, V8_ENUM_BIT_TYPE_NAME(enum_name) val2) { \
    return (static_cast<V8_ENUM_BIT_TYPE_NAME(enum_name)>(val1) & val2) ; \
  } \
  V8_ENUM_BIT_TYPE_NAME(enum_name) operator &( \
      V8_ENUM_BIT_TYPE_NAME(enum_name) val1, enum_name val2) { \
    return (val2 & val1) ; \
  } \
  V8_ENUM_BIT_TYPE_NAME(enum_name) operator |( \
      enum_name val1, enum_name val2) { \
    return (static_cast<V8_ENUM_BIT_TYPE_NAME(enum_name)>(val1) | \
            static_cast<V8_ENUM_BIT_TYPE_NAME(enum_name)>(val2)) ; \
  } \
  V8_ENUM_BIT_TYPE_NAME(enum_name) operator |( \
      enum_name val1, V8_ENUM_BIT_TYPE_NAME(enum_name) val2) { \
    return (static_cast<V8_ENUM_BIT_TYPE_NAME(enum_name)>(val1) | val2) ; \
  } \
  V8_ENUM_BIT_TYPE_NAME(enum_name) operator |( \
      V8_ENUM_BIT_TYPE_NAME(enum_name) val1, enum_name val2) { \
    return (val2 | val1) ; \
  }

namespace v8 {
namespace vm {
namespace internal {

// Converts a utf8-string into a internal V8 string
static inline Local<String> Utf8ToStr(Isolate* isolate, const char* str) {
  Local<String> result ;
  TryCatch try_catch(isolate) ;
  if (!String::NewFromUtf8(isolate, str, NewStringType::kNormal)
          .ToLocal(&result)) {
    printf("ERROR: Can't convert utf8-string to v8::String\n") ;
    return Local<String>() ;
  }

  return result ;
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
