// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_VM_VALUE_H_
#define V8_VM_VM_VALUE_H_

#include "src/vm/utils/vm-utils.h"

namespace v8 {
namespace vm {
namespace internal {

#define V8_VALUE_ENUM_ITEM_LIST(V) \
  V Undefined, V8_ENUM_BIT_NO_PARENT, "The undefined value (ECMA-262 4.3.10)") \
  V Null, V8_ENUM_BIT_NO_PARENT, "The null value (ECMA-262 4.3.11)") \
  V String, V8_ENUM_BIT_NO_PARENT, "String type (ECMA-262 8.4)") \
  V Symbol, V8_ENUM_BIT_NO_PARENT, "Symbol") \
  V Object, V8_ENUM_BIT_NO_PARENT, "Array") \
  V Function, Object, "Function") \
  V Array, Object, "Array") \
  V BigInt, V8_ENUM_BIT_NO_PARENT, "Bigint") \
  V Boolean, V8_ENUM_BIT_NO_PARENT, "Boolean") \
  V Number, V8_ENUM_BIT_NO_PARENT, "Number") \
  V External, V8_ENUM_BIT_NO_PARENT, "External") \
  V Int32, Number, "32-bit signed integer") \
  V Uint32, Number, "32-bit unsigned integer") \
  V Date, Object, "Date") \
  V ArgumentsObject, Object, "Arguments object") \
  V BigIntObject, Object, "BigInt object") \
  V BooleanObject, Object, "Boolean object") \
  V NumberObject, Object, "Number object") \
  V StringObject, Object, "String object") \
  V SymbolObject, Object, "Symbol object") \
  V NativeError, V8_ENUM_BIT_NO_PARENT, "NativeError") \
  V RegExp, Object, "RegExp") \
  V AsyncFunction, Function, "Async function") \
  V GeneratorFunction, Object, "Generator function") \
  V GeneratorObject, Object, "Generator object (iterator)") \
  V Promise, Object, "Promise") \
  V Map, Object, "Map") \
  V Set, Object, "Set") \
  V MapIterator, Object, "Map Iterator") \
  V SetIterator, Object, "Set Iterator") \
  V WeakMap, Object, "WeakMap") \
  V WeakSet, Object, "WeakSet") \
  V ArrayBuffer, Object, "ArrayBuffer") \
  V ArrayBufferView, Object, "ArrayBufferView") \
  V Uint8Array, ArrayBufferView, "Uint8Array") \
  V Uint8ClampedArray, ArrayBufferView, "Uint8ClampedArray") \
  V Int8Array, ArrayBufferView, "Int8Array") \
  V Uint16Array, ArrayBufferView, "Uint16Array") \
  V Int16Array, ArrayBufferView, "Int16Array") \
  V Uint32Array, ArrayBufferView, "Uint32Array") \
  V Int32Array, ArrayBufferView, "Int32Array") \
  V Float32Array, ArrayBufferView, "Float32Array") \
  V Float64Array, ArrayBufferView, "Float64Array") \
  V BigInt64Array, ArrayBufferView, "BigInt64Array") \
  V BigUint64Array, ArrayBufferView, "BigUint64Array") \
  V DataView, ArrayBufferView, "DataView") \
  V SharedArrayBuffer, Object, \
      "SharedArrayBuffer (This is an experimental feature)") \
  V Proxy, Object, "JavaScript Proxy") \
  V WebAssemblyCompiledModule, Object, "WebAssemblyCompiledModule") \
  V ModuleNamespaceObject, Object, "Module Namespace Object")

V8_ENUM_BIT_DECLARE_HELPER(ValueType, std::uint64_t, V8_VALUE_ENUM_ITEM_LIST)

enum class ValueType : std::uint64_t {
  Unknown = 0,

  V8_ENUM_BIT_DECLARE_ITEMS_WITH_PARENT(ValueType, V8_VALUE_ENUM_ITEM_LIST)

  // Number types
  NumberTypes = V8_ENUM_BIT_GET_ITEM_BIT(ValueType, Number) |
                V8_ENUM_BIT_GET_ITEM_BIT(ValueType, Int32) |
                V8_ENUM_BIT_GET_ITEM_BIT(ValueType, Uint32),

  // Primitive object types
  PrimitiveObjectTypes = V8_ENUM_BIT_GET_ITEM_BIT(ValueType, BigIntObject) |
                         V8_ENUM_BIT_GET_ITEM_BIT(ValueType, BooleanObject) |
                         V8_ENUM_BIT_GET_ITEM_BIT(ValueType, NumberObject) |
                         V8_ENUM_BIT_GET_ITEM_BIT(ValueType, StringObject) |
                         V8_ENUM_BIT_GET_ITEM_BIT(ValueType, SymbolObject),
};

V8_ENUM_BIT_OPERATORS_DECLARATION(ValueType)

static inline ValueType GetValueType(Value* value) {
  #define V8_CHECK_VALUE_TYPE(val, result, type, ...) \
    if (val->Is##type() && V8_ENUM_BIT_IS_PARENT(result,  ValueType::type)) { \
      result = ValueType::type ; \
    }

  if (!value) {
    DCHECK(false) ;
    return ValueType::Unknown ;
  }

  ValueType result = ValueType::Unknown ;
  V8_ENUM_INVOKE_ON_LIST_WITH_ARGS(
      V8_CHECK_VALUE_TYPE, V8_VALUE_ENUM_ITEM_LIST, value, result)

  DCHECK_NE(ValueType::Unknown, result) ;
  return result ;
}

static inline ValueType GetValueType(Local<Value> value) {
  return GetValueType(value.operator ->()) ;
}

static inline const char* ValueTypeToUtf8(ValueType type) {
  #define V8_TYPE_TO_STRING(ckecked_type, type, ...) \
    if (ckecked_type == ValueType::type) { return #type ; }

  V8_ENUM_INVOKE_ON_LIST_WITH_ARGS(
      V8_TYPE_TO_STRING, V8_VALUE_ENUM_ITEM_LIST, type)

  DCHECK(false) ;
  return "Unknown" ;
}

}  // namespace internal
}  // namespace vm
}  // namespace v8

#endif  // V8_VM_VM_VALUE_H_
