// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/vm/dumper.h"

#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <set>

#include "src/api.h"
#include "src/handles-inl.h"
#include "src/vm/utils/json-utils.h"
#include "src/vm/utils/vm-utils.h"
#include "src/vm/vm-value.h"

namespace v8 {
namespace vm {
namespace internal {

namespace {

// Json defines
#define JSON_SYMBOL_FIELD(name, gap) \
  JSON_FIELD(("Symbol(" + name + ")"), gap)

typedef std::set<SnapshotObjectId> ProcessedNodeArray ;

// Json field name list
const char* kJsonFieldAllocationData[] = JSON_ARRAY_OF_FIELD(allocation_data) ;
const char* kJsonFieldAllocationLength[] =
    JSON_ARRAY_OF_FIELD(allocation_length) ;
const char* kJsonFieldAllocationMode[] = JSON_ARRAY_OF_FIELD(allocation_mode) ;
const char* kJsonFieldArrayBufferView[] =
    JSON_ARRAY_OF_FIELD(__array_buffer_view__) ;
const char* kJsonFieldBuffer[] = JSON_ARRAY_OF_FIELD(buffer) ;
const char* kJsonFieldConstructorName[] =
    JSON_ARRAY_OF_FIELD(constructor_name) ;
const char* kJsonFieldData[] = JSON_ARRAY_OF_FIELD(data) ;
const char* kJsonFieldDebugName[] = JSON_ARRAY_OF_FIELD(debug_name) ;
const char* kJsonFieldDisplayName[] = JSON_ARRAY_OF_FIELD(display_name) ;
const char* kJsonFieldEdges[] = JSON_ARRAY_OF_FIELD(edges) ;
const char* kJsonFieldFlags[] = JSON_ARRAY_OF_FIELD(flags) ;
const char* kJsonFieldFunction[] = JSON_ARRAY_OF_FIELD(__function__) ;
const char* kJsonFieldHasHandler[] = JSON_ARRAY_OF_FIELD(has_handler) ;
const char* kJsonFieldId[] = JSON_ARRAY_OF_FIELD(id) ;
const char* kJsonFieldIndex[] = JSON_ARRAY_OF_FIELD(index) ;
const char* kJsonFieldInferredName[] = JSON_ARRAY_OF_FIELD(inferred_name) ;
const char* kJsonFieldInternalFieldCount[] =
    JSON_ARRAY_OF_FIELD(internal_field_count) ;
const char* kJsonFieldInternalFields[] = JSON_ARRAY_OF_FIELD(internal_fields) ;
const char* kJsonFieldIsExternal[] = JSON_ARRAY_OF_FIELD(is_external) ;
const char* kJsonFieldIsNeuterable[] = JSON_ARRAY_OF_FIELD(is_neuterable) ;
const char* kJsonFieldKey[] = JSON_ARRAY_OF_FIELD(key) ;
const char* kJsonFieldLength[] = JSON_ARRAY_OF_FIELD(length) ;
const char* kJsonFieldName[] = JSON_ARRAY_OF_FIELD(name) ;
const char* kJsonFieldNativeError[] = JSON_ARRAY_OF_FIELD(native_error) ;
const char* kJsonFieldNode[] = JSON_ARRAY_OF_FIELD(node) ;
const char* kJsonFieldNodeCount[] = JSON_ARRAY_OF_FIELD(node_count) ;
const char* kJsonFieldNodes[] = JSON_ARRAY_OF_FIELD(nodes) ;
const char* kJsonFieldObject[] = JSON_ARRAY_OF_FIELD(__object__) ;
const char* kJsonFieldOffset[] = JSON_ARRAY_OF_FIELD(offset) ;
const char* kJsonFieldProcessed[] = JSON_ARRAY_OF_FIELD(processed) ;
const char* kJsonFieldProperties[] = JSON_ARRAY_OF_FIELD(properties) ;
const char* kJsonFieldPropertyCount[] = JSON_ARRAY_OF_FIELD(property_count) ;
const char* kJsonFieldProto[] = JSON_ARRAY_OF_FIELD(__proto__) ;
const char* kJsonFieldResourceName[] = JSON_ARRAY_OF_FIELD(resource_name) ;
const char* kJsonFieldResult[] = JSON_ARRAY_OF_FIELD(result) ;
const char* kJsonFieldScriptLine[] = JSON_ARRAY_OF_FIELD(script_line) ;
const char* kJsonFieldScriptColumn[] = JSON_ARRAY_OF_FIELD(script_column) ;
const char* kJsonFieldSize[] = JSON_ARRAY_OF_FIELD(size) ;
const char* kJsonFieldSource[] = JSON_ARRAY_OF_FIELD(source) ;
const char* kJsonFieldState[] = JSON_ARRAY_OF_FIELD(state) ;
const char* kJsonFieldToString[] = JSON_ARRAY_OF_FIELD(to_string) ;
const char* kJsonFieldType[] = JSON_ARRAY_OF_FIELD(type) ;
const char* kJsonFieldTypedArray[] = JSON_ARRAY_OF_FIELD(__typed_array__) ;
const char* kJsonFieldUndefinedType[] =
    JSON_ARRAY_OF_FIELD([undefined field type]) ;
const char* kJsonFieldValue[] = JSON_ARRAY_OF_FIELD(value) ;

// Json value list
const char kJsonValueException[] = R"("[exception]")" ;
const char kJsonValueFalse[] = "false" ;
const char kJsonValueInfinity[] = R"("Infinity")" ;
const char kJsonValueInvalid[] = R"("[invalid]")" ;
const char kJsonValueNaN[] = R"("NaN")" ;
const char kJsonValueNegativeInfinity[] = R"("-Infinity")" ;
const char kJsonValueNull[] = "null" ;
const char kJsonValueTrue[] = "true" ;
const char kJsonValueUnknown[] = R"("[unknown]")" ;
const char kJsonValueUndefined[] = R"("[undefined]")" ;

// Heap edge type list
//
// A variable from a function context
const char kEdgeTypeContextVariable[] = R"("ContextVariable")" ;
// An element of an array
const char kEdgeTypeElement[] = R"("Element")" ;
// A link that is needed for proper sizes calculation,
// but may be hidden from user
const char kEdgeTypeHidden[] = R"("Hidden")" ;
// A link that can't be accessed from JS, thus,
// its name isn't a real property name (e.g. parts of a ConsString)
const char kEdgeTypeInternal[] = R"("Internal")" ;
// A named object property
const char kEdgeTypeProperty[] = R"("Property")" ;
// A link that must not be followed during sizes calculation
const char kEdgeTypeShortcut[] = R"("Shortcut")" ;
// A weak reference (ignored by the GC)
const char kEdgeTypeWeak[] = R"("Weak")" ;
// Unknown
const char kEdgeTypeUnknown[] = R"("Unknown")" ;

// Heap node type list
//
// An array of elements
const char kNodeTypeArray[] = R"("Array")" ;
// BigInt
const char kNodeTypeBigInt[] = R"("BigInt")" ;
// Function closure
const char kNodeTypeClosure[] = R"("Closure")" ;
// Compiled code
const char kNodeTypeCode[] = R"("Code")" ;
// Concatenated string. A pair of pointers to strings
const char kNodeTypeConsString[] = R"("ConsString")" ;
// Number stored in the heap
const char kNodeTypeHeapNumber[] = R"("HeapNumber")" ;
// Hidden node, may be filtered when shown to user
const char kNodeTypeHidden[] = R"("Hidden")" ;
// Native object (not from V8 heap)
const char kNodeTypeNative[] = R"("Native")" ;
// A JS object (except for arrays and strings)
const char kNodeTypeObject[] = R"("Object")" ;
// RegExp
const char kNodeTypeRegExp[] = R"("RegExp")" ;
// Sliced string. A fragment of another string
const char kNodeTypeSlicedString[] = R"("SlicedString")" ;
// A string
const char kNodeTypeString[] = R"("String")" ;
// Synthetic object, usually used for grouping snapshot items together
const char kNodeTypeSynthetic[] = R"("Synthetic")" ;
// A Symbol (ES6)
const char kNodeTypeSymbol[] = R"("Symbol")" ;
// Unknown
const char kNodeTypeUnknown[] = R"("Unknown")" ;

// Class-helper for a heap serialization by V8 API
class HeapSerializer : public OutputStream {
 public:
  HeapSerializer(std::ostream& result)
    : result_(result) {}

  void EndOfStream() override {}

  int GetChunkSize() override { return 1024 * 1024 ; }

  WriteResult WriteAsciiChunk(char* data, int size) override {
    result_.write(data, size) ;
    return kContinue ;
  }

  WriteResult WriteHeapStatsChunk(HeapStatsUpdate* data, int count) override {
    return kContinue ;
  }

 private:
  std::ostream& result_ ;
};

// Value serializer
class ValueSerializer {
 public:
  ValueSerializer(Local<Context> context, FormattedJson default_format) ;

  void Serialize(
      Local<Value> value, std::ostream& result, JsonGap* gap = nullptr) ;

 private:
  // Serializes v8::Value
  void SerializeValue(Local<Value> value, const JsonGap& gap) ;

  // Functions for serializing particular types of v8::Value
  void SerializeArgumentsObject(
      Local<Object> value, uint64_t id, const JsonGap& gap) ;
  void SerializeArray(Local<Array> value, uint64_t id, const JsonGap& gap) ;
  void SerializeArrayBuffer(
      Local<ArrayBuffer> value, uint64_t id, const JsonGap& gap) ;
  void SerializeArrayBufferView(
      Local<ArrayBufferView> value, uint64_t id, const JsonGap& gap) ;
  void SerializeAsyncFunction(
      Local<Function> value, uint64_t id, const JsonGap& gap) ;
  void SerializeBigInt(Local<BigInt> value, uint64_t id, const JsonGap& gap) ;
  void SerializeBoolean(Local<Boolean> value, uint64_t id, const JsonGap& gap) ;
  void SerializeDataView(
      Local<DataView> value, uint64_t id, const JsonGap& gap) ;
  void SerializeDate(Local<Date> value, uint64_t id, const JsonGap& gap) ;
  void SerializeFunction(
      Local<Function> value, uint64_t id, const JsonGap& gap) ;
  void SerializeGeneratorFunction(
      Local<Function> value, uint64_t id, const JsonGap& gap) ;
  void SerializeGeneratorObject(
      Local<Object> value, uint64_t id, const JsonGap& gap) ;
  void SerializeMap(Local<Map> value, uint64_t id, const JsonGap& gap) ;
  void SerializeMapIterator(
      Local<Object> value, uint64_t id, const JsonGap& gap) ;
  void SerializeNumber(Local<Number> value, uint64_t id, const JsonGap& gap) ;
  void SerializeObject(Local<Object> value, uint64_t id, const JsonGap& gap) ;
  void SerializePrimitiveObject(
      Local<Object> value, uint64_t id, const JsonGap& gap) ;
  void SerializeProcessedValue(
      Local<Value> value, uint64_t id, const JsonGap& gap) ;
  void SerializePromise(Local<Promise> value, uint64_t id, const JsonGap& gap) ;
  void SerializeRegExp(Local<RegExp> value, uint64_t id, const JsonGap& gap) ;
  void SerializeSet(Local<Set> value, uint64_t id, const JsonGap& gap) ;
  void SerializeSetIterator(
      Local<Object> value, uint64_t id, const JsonGap& gap) ;
  void SerializeString(Local<String> value, uint64_t id, const JsonGap& gap) ;
  void SerializeSymbol(Local<Symbol> value, uint64_t id, const JsonGap& gap) ;
  void SerializeTypedArray(
      Local<TypedArray> value, uint64_t id, const JsonGap& gap) ;
  void SerializeTypedArrayObject(
      Local<TypedArray> value, uint64_t id, const JsonGap& gap) ;
  void SerializeWeakMap(Local<Object> value, uint64_t id, const JsonGap& gap) ;
  void SerializeWeakSet(Local<Object> value, uint64_t id, const JsonGap& gap) ;

  // Serializes common fields
  // Returns 'true' if it have done something
  bool SerializeCommonFileds(
      uint64_t id, ValueType type, Value* value, const JsonGap& gap) ;

  // Serializes an exception into json-value
  void SerializeException(TryCatch& try_catch, const JsonGap& gap) ;

  // Serializes an array as a map into json-value
  void SerializeKeyValueArray(Local<Array> value, const JsonGap& gap) ;

  // Serializes an array into json-value
  void SerializeValueArray(Local<Array> value, const JsonGap& gap) ;

  // Converts v8::Value into a json field name
  std::string ValueToField(Local<Value> value, const JsonGap& gap) ;

  Local<Context> context_ ;
  JsonGap default_gap_ ;
  std::map<i::Object*, uint64_t> processed_values_ ;
  std::ostream* result_ = nullptr ;

  static const uint64_t kEmptyId ;

  DISALLOW_COPY_AND_ASSIGN(ValueSerializer) ;
};

// Converts double to string
std::string DoubleToUtf8(double val) {
  if (std::isnan(val)) {
    return kJsonValueNaN ;
  } else if (std::isinf(val)) {
    return (val == std::numeric_limits<double>::infinity() ?
                kJsonValueInfinity : kJsonValueNegativeInfinity) ;
  }

  // TODO: Use "string-number-conversions.h": DoubleToString(...)
  return std::to_string(val) ;
}

// Checks a string is a number
bool IsNumber(const char* str) {
  if (*str == '-' || *str == '+') { ++str; }
  if (!std::isdigit(*str)) {
    return false;
  }

  ++str ;
  while (std::isdigit(*str)) { ++str; }
  if (*str == '.') {
    ++str ;
    if (!std::isdigit(*str)) {
      return false ;
    }

    while (std::isdigit(*str)) { ++str; }
  }

  if (*str == 'e' || *str == 'E') {
    ++str ;
    if (*str == '-' || *str == '+') { ++str; }
    if (!std::isdigit(*str)) {
      return false ;
    }

    while (std::isdigit(*str)) { ++str; }
  }

  return *str == '\0' ;
}

std::string HexEncode(const void* bytes, size_t size) {
  static const char kHexChars[] = "0123456789abcdef" ;

  // Each input byte creates two output hex characters.
  std::string ret(size * 2, '\0') ;

  for (size_t i = 0; i < size; ++i) {
    char b = reinterpret_cast<const char*>(bytes)[i] ;
    ret[(i * 2)] = kHexChars[(b >> 4) & 0xf] ;
    ret[(i * 2) + 1] = kHexChars[b & 0xf] ;
  }

  return ret ;
}

const uint64_t ValueSerializer::kEmptyId = static_cast<uint64_t>(-1) ;

ValueSerializer::ValueSerializer(
    Local<Context> context, FormattedJson default_format)
  : context_(context), default_gap_(default_format, 0) {}

void ValueSerializer::Serialize(
    Local<Value> value, std::ostream& result, JsonGap* gap /*= nullptr*/) {
  if (!gap) {
    gap = &default_gap_ ;
  }

  result_ = &result ;
  SerializeValue(value, *gap) ;
  result_ = nullptr ;
}

void ValueSerializer::SerializeValue(Local<Value> value, const JsonGap& gap) {
  ValueType value_type = GetValueType(value) ;
  if (value.IsEmpty() || value_type == ValueType::Null) {
    *result_ << kJsonValueNull ;
    return ;
  }

  // Check value that we've alredy processed it
  i::Handle<i::Object> obj = Utils::OpenHandle(value.operator ->()) ;
  auto it = processed_values_.find(obj.operator ->()) ;
  if (it != processed_values_.end()) {
    SerializeProcessedValue(value, it->second, gap) ;
    return ;
  }

  // Save value into processed values
  uint64_t id = processed_values_.size() ;
  processed_values_[obj.operator ->()] = id ;

  if (value_type == ValueType::Undefined) {
    *result_ << kJsonValueUndefined ;
    return ;
  } else if (value_type == ValueType::ArgumentsObject) {
    SerializeArgumentsObject(Local<Object>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::Array) {
    SerializeArray(Local<Array>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::ArrayBuffer) {
    SerializeArrayBuffer(Local<ArrayBuffer>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::ArrayBufferView) {
    SerializeArrayBufferView(Local<ArrayBufferView>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::AsyncFunction) {
    SerializeAsyncFunction(Local<Function>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::BigInt) {
    SerializeBigInt(Local<BigInt>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::Boolean) {
    SerializeBoolean(Local<Boolean>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::DataView) {
    SerializeDataView(Local<DataView>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::Date) {
    SerializeDate(Local<Date>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::Function) {
    SerializeFunction(Local<Function>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::GeneratorFunction) {
    SerializeGeneratorFunction(Local<Function>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::GeneratorObject) {
    SerializeGeneratorObject(Local<Object>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::Map) {
    SerializeMap(Local<Map>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::MapIterator) {
    SerializeMapIterator(Local<Object>::Cast(value), id, gap) ;
    return ;
  } else if (value_type & ValueType::NumberTypes) {
    SerializeNumber(Local<Number>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::Object) {
    SerializeObject(Local<Object>::Cast(value), id, gap) ;
    return ;
  } else if (value_type & ValueType::PrimitiveObjectTypes) {
    SerializePrimitiveObject(Local<Object>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::Promise) {
    SerializePromise(Local<Promise>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::RegExp) {
    SerializeRegExp(Local<RegExp>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::Set) {
    SerializeSet(Local<Set>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::SetIterator) {
    SerializeSetIterator(Local<Object>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::String) {
    SerializeString(Local<String>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::Symbol) {
    SerializeSymbol(Local<Symbol>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::TypedArray) {
    SerializeTypedArray(Local<TypedArray>::Cast(value), id, gap) ;
    return ;
  } else if (value_type & ValueType::TypedArrayTypes) {
    SerializeTypedArrayObject(Local<TypedArray>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::WeakMap) {
    SerializeWeakMap(Local<Object>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::WeakSet) {
    SerializeWeakSet(Local<Object>::Cast(value), id, gap) ;
    return ;
  }

  V8_LOG_ERR(
      errUnknown, "Don't have a serializer for \'%s\'",
      ValueTypeToUtf8(value_type)) ;
  *result_ << kJsonValueUnknown ;
}

void ValueSerializer::SerializeArgumentsObject(
    Local<Object> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(
      id, ValueType::ArgumentsObject, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  // Serialize Object
  *result_ << child_gap << kJsonFieldObject[gap] ;
  SerializeObject(value, kEmptyId, child_gap) ;

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeArray(
    Local<Array> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, ValueType::Array, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  *result_ << child_gap << kJsonFieldLength[gap] << value->Length() ;

  // Serialize a array content
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldValue[gap] ;
  SerializeValueArray(value, child_gap) ;

  // Serialize Object
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldObject[gap] ;
  SerializeObject(value, kEmptyId, child_gap) ;

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeArrayBuffer(
    Local<ArrayBuffer> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, ValueType::ArrayBuffer, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  // Serialize value
  ArrayBuffer::Contents contents = value->GetContents() ;
  *result_ << child_gap << kJsonFieldAllocationMode[gap]
      << (contents.AllocationMode() ==
              ArrayBuffer::Allocator::AllocationMode::kNormal ?
          "\"Normal\"" : "\"Reservation\"") ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldData[gap]
      << JSON_STRING(HexEncode(contents.Data(), contents.ByteLength())) ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldLength[gap]
      << contents.ByteLength() ;
  if (contents.Data() != contents.AllocationBase() ||
      contents.ByteLength() != contents.AllocationLength()) {
    *result_ << kJsonComma[gap] << child_gap << kJsonFieldAllocationData[gap]
        << JSON_STRING(HexEncode(
               contents.AllocationBase(), contents.AllocationLength())) ;
    *result_ << kJsonComma[gap] << child_gap << kJsonFieldAllocationLength[gap]
        << contents.AllocationLength() ;
  }

  *result_ << kJsonComma[gap] << child_gap << kJsonFieldIsExternal[gap]
      << (value->IsExternal() ? kJsonValueTrue : kJsonValueFalse) ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldIsNeuterable[gap]
      << (value->IsNeuterable() ? kJsonValueTrue : kJsonValueFalse) ;

  // Serialize Object
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldObject[gap] ;
  SerializeObject(value, kEmptyId, child_gap) ;

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeArrayBufferView(
    Local<ArrayBufferView> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(
          id, ValueType::ArrayBufferView, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  // Serialize value
  *result_ << child_gap << kJsonFieldOffset[gap]
      << value->ByteOffset() ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldLength[gap]
      << value->ByteLength() ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldBuffer[gap] ;
  SerializeValue(value->Buffer(), child_gap) ;

  // Serialize Object
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldObject[gap] ;
  SerializeObject(value, kEmptyId, child_gap) ;

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeAsyncFunction(
    Local<Function> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, ValueType::AsyncFunction, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  // Serialize Function
  *result_ << child_gap << kJsonFieldFunction[gap] ;
  SerializeFunction(value, kEmptyId, child_gap) ;

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeBigInt(
    Local<BigInt> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, ValueType::BigInt, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  *result_ << child_gap << kJsonFieldValue[gap] ;
  std::string value_str = ValueToUtf8(context_, value) ;
  if (IsNumber(value_str.c_str())) {
    *result_ << value_str ;
  } else {
    *result_  << JSON_STRING(value_str) ;
  }

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeBoolean(
    Local<Boolean> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, ValueType::Boolean, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  *result_ << child_gap << kJsonFieldValue[gap]
      << (value->Value() ? kJsonValueTrue : kJsonValueFalse) ;
  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeDataView(
    Local<DataView> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, ValueType::DataView, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  // Serialize ArrayBufferView
  *result_ << child_gap << kJsonFieldArrayBufferView[gap] ;
  SerializeArrayBufferView(value, kEmptyId, child_gap) ;

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeDate(
    Local<Date> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, ValueType::Date, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  *result_ << child_gap << kJsonFieldValue[gap]
      << DoubleToUtf8(value->ValueOf()) ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldToString[gap]
      << JSON_STRING(ValueToUtf8(context_, value).c_str()) ;

  // Serialize Object
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldObject[gap] ;
  SerializeObject(value, kEmptyId, child_gap) ;

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeFunction(
    Local<Function> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, ValueType::Function, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  *result_ << child_gap << kJsonFieldToString[gap]
      << JSON_STRING(ValueToUtf8(context_, value)) ;
  *result_ << kJsonComma[gap] ;

  // Function name
  std::string name = ValueToUtf8(context_, value->GetName()) ;
  *result_ << child_gap << kJsonFieldName[gap] << JSON_STRING(name) ;

  // Name inferred from variable or property assignment of this function.
  // Used to facilitate debugging and profiling of JavaScript code written
  // in an OO style, where many functions are anonymous but are assigned
  // to object properties.
  Local<Value> inferred_name_value = value->GetInferredName() ;
  std::string inferred_name = ValueToUtf8(context_, inferred_name_value) ;
  if (inferred_name.length() && inferred_name != name &&
      !inferred_name_value.IsEmpty() && !inferred_name_value->IsUndefined()) {
    *result_ << kJsonComma[gap] << child_gap ;
    *result_ << kJsonFieldInferredName[gap] << JSON_STRING(inferred_name) ;
  }

  // displayName if it is set, otherwise name if it is configured, otherwise
  // function name, otherwise inferred name.
  Local<Value> debug_name_value = value->GetDebugName() ;
  std::string debug_name = ValueToUtf8(context_, debug_name_value) ;
  if (debug_name.length() && debug_name != name &&
      debug_name != inferred_name && !debug_name_value.IsEmpty() &&
      !debug_name_value->IsUndefined()) {
    *result_ << kJsonComma[gap] << child_gap ;
    *result_ << kJsonFieldDebugName[gap] << JSON_STRING(debug_name) ;
  }

  // User-defined name assigned to the "displayName" property of this function.
  // Used to facilitate debugging and profiling of JavaScript code.
  Local<Value> display_name_value = value->GetDisplayName() ;
  std::string display_name = ValueToUtf8(context_, display_name_value) ;
  if (display_name.length() && display_name != name &&
      display_name != inferred_name && display_name != debug_name &&
      !display_name_value.IsEmpty() && !display_name_value->IsUndefined()) {
    *result_ << kJsonComma[gap] << child_gap ;
    *result_ << kJsonFieldDisplayName[gap] << JSON_STRING(display_name) ;
  }

  // Script position
  if (value->GetScriptLineNumber() != -1 ||
      value->GetScriptColumnNumber() != -1) {
    *result_ << kJsonComma[gap] << child_gap ;
    *result_ << kJsonFieldScriptLine[gap] << value->GetScriptLineNumber() ;
    *result_ << kJsonComma[gap] << child_gap ;
    *result_ << kJsonFieldScriptColumn[gap] << value->GetScriptColumnNumber() ;
  }

  // Script resource name
  std::string resource_name =
      ValueToUtf8(context_, value->GetScriptOrigin().ResourceName()) ;
  if (resource_name.length()) {
    *result_ << kJsonComma[gap] << child_gap ;
    *result_ << kJsonFieldResourceName[gap] << JSON_STRING(resource_name) ;
  }

  // Serialize Object
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldObject[gap] ;
  SerializeObject(value, kEmptyId, child_gap) ;

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeGeneratorFunction(
    Local<Function> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(
          id, ValueType::GeneratorFunction, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  // Serialize Function
  *result_ << child_gap << kJsonFieldFunction[gap] ;
  SerializeFunction(value, kEmptyId, child_gap) ;

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeGeneratorObject(
    Local<Object> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(
          id, ValueType::GeneratorObject, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  // Serialize Object
  *result_ << child_gap << kJsonFieldObject[gap] ;
  SerializeObject(value, kEmptyId, child_gap) ;

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeMap(
    Local<Map> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, ValueType::Map, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  *result_ << child_gap << kJsonFieldSize[gap] << value->Size() ;
  *result_ << kJsonComma[gap] ;

  // Serialize content
  *result_ << child_gap << kJsonFieldValue[gap] ;
  Local<Array> map = value->AsArray() ;
  SerializeKeyValueArray(map, child_gap) ;

  // Serialize Object
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldObject[gap] ;
  SerializeObject(value, kEmptyId, child_gap) ;

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeMapIterator(
    Local<Object> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, ValueType::MapIterator, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  // Serialize value
  bool is_key_value = false ;
  *result_ << child_gap << kJsonFieldValue[gap] ;
  TryCatch try_catch(context_->GetIsolate()) ;
  Local<Array> iterator_value ;
  if (!value->PreviewEntries(&is_key_value).ToLocal(&iterator_value)) {
    SerializeException(try_catch, child_gap) ;
  } else if (iterator_value.IsEmpty() && iterator_value->Length() == 0) {
    *result_ << kJsonValueNull ;
  } else if (!is_key_value || (iterator_value->Length() & 1)) {
    *result_ << kJsonValueInvalid ;
  } else {
    JsonGap value_gap(child_gap) ;
    *result_ << kJsonLeftBracket[gap] ;

    // Serialize key
    *result_ << value_gap << kJsonFieldKey[gap] ;
    v8::Local<v8::Value> key_value ;
    if (!iterator_value->Get(context_, 0).ToLocal(&key_value)) {
      SerializeException(try_catch, value_gap) ;
    } else {
      SerializeValue(key_value, value_gap) ;
    }

    *result_ << kJsonComma[gap] ;

    // Serialize value
    *result_ << value_gap << kJsonFieldValue[gap] ;
    v8::Local<v8::Value> value_value ;
    if (!iterator_value->Get(context_, 1).ToLocal(&value_value)) {
      SerializeException(try_catch, value_gap) ;
    } else {
      SerializeValue(value_value, value_gap) ;
    }

    *result_ << kJsonNewLine[gap] << child_gap << kJsonRightBracket[gap] ;
  }

  // Serialize Object
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldObject[gap] ;
  SerializeObject(value, kEmptyId, child_gap) ;

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeNumber(
    Local<Number> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, GetValueType(value), *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  *result_ << child_gap << kJsonFieldValue[gap] ;
  std::string value_str = ValueToUtf8(context_, value) ;
  if (IsNumber(value_str.c_str())) {
    *result_ << value_str ;
  } else {
    *result_  << JSON_STRING(value_str) ;
  }

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeObject(
    Local<Object> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  JsonGap array_gap(child_gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, ValueType::Object, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  *result_ << child_gap << kJsonFieldConstructorName[gap]
      << JSON_STRING(ValueToUtf8(context_, value->GetConstructorName())) ;

  // Count of ignored properties
  ValueType real_type = GetValueType(value) ;
  uint32_t ignored_properties_count = 0 ;
  if (id == kEmptyId && real_type == ValueType::Array) {
    ignored_properties_count = Local<Array>::Cast(value)->Length() ;
  }

  // Enumerate own properties
  v8::Local<v8::Array> property_names ;
  if (value->GetOwnPropertyNames(context_, ALL_PROPERTIES)
          .ToLocal(&property_names) &&
      !property_names.IsEmpty() &&
      (property_names->Length() - ignored_properties_count) > 0) {
    *result_ << kJsonComma[gap] ;
    *result_ << child_gap << kJsonFieldPropertyCount[gap]
        << (property_names->Length() - ignored_properties_count) ;
    *result_ << kJsonComma[gap] << child_gap ;
    *result_ << kJsonFieldProperties[gap] << kJsonLeftBracket[gap] ;
    bool comma = false ;
    for (uint32_t i = 0, size = property_names->Length(); i < size; ++i) {
      v8::Local<v8::Value> key ;
      if (!property_names->Get(context_, i).ToLocal(&key) || key.IsEmpty()) {
        DCHECK(false) ;
        V8_LOG_ERR(errUnknown, "Can't get a property name") ;
        continue ;
      }

      // Check that we don't ignore it
      if (id == kEmptyId && real_type == ValueType::Array) {
        ValueType key_type = GetValueType(key) ;
        if (key_type == ValueType::Int32 || key_type == ValueType::Uint32) {
          uint32_t key_value = (uint32_t)Local<Number>::Cast(key)->Value() ;
          if (key_value < ignored_properties_count) {
            continue ;
          }
        }
      }

      if (comma) {
        *result_ << kJsonComma[gap] ;
      } else {
        comma = true ;
      }

      *result_ << array_gap << ValueToField(key, gap) ;

      TryCatch try_catch(context_->GetIsolate()) ;
      v8::Local<v8::Value> child_value ;
      if (!value->Get(context_, key).ToLocal(&child_value)) {
        SerializeException(try_catch, array_gap) ;
        continue ;
      }

      SerializeValue(child_value, array_gap) ;
    }

    *result_ << kJsonNewLine[gap] << child_gap << kJsonRightBracket[gap] ;
  }

  // Enumerate internal fields
  if (value->InternalFieldCount()) {
    *result_ << kJsonComma[gap] ;
    *result_ << child_gap << kJsonFieldInternalFieldCount[gap]
        << value->InternalFieldCount() ;
    *result_ << kJsonComma[gap] ;
    *result_ << child_gap << kJsonFieldInternalFields[gap]
        << kJsonLeftSquareBracket[gap] ;
    bool comma = false ;
    for (uint32_t i = 0, size = value->InternalFieldCount(); i < size; ++i) {
      if (comma) {
        *result_ << kJsonComma[gap] ;
      } else {
        comma = true ;
      }

      *result_ << array_gap ;
      v8::Local<v8::Value> child_value = value->GetInternalField(i) ;
      SerializeValue(child_value, array_gap) ;
    }

    *result_ << kJsonNewLine[gap] << child_gap << kJsonRightSquareBracket[gap] ;
  }

  // Serialize a prototype
  Local<Value> prototype = value->GetPrototype() ;
  if (!prototype.IsEmpty() && !prototype->IsNull()) {
    *result_ << kJsonComma[gap] ;
    *result_ << child_gap << kJsonFieldProto[gap] ;
    SerializeValue(prototype, child_gap) ;
  }

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializePrimitiveObject(
    Local<Object> value, uint64_t id, const JsonGap& gap) {
  ValueType value_type = GetValueType(value) ;
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, value_type, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  *result_ << child_gap << kJsonFieldValue[gap] ;

  // Serialize value
  if (value_type == ValueType::BigIntObject) {
    Local<BigIntObject> real_value = Local<BigIntObject>::Cast(value) ;
    SerializeValue(real_value->ValueOf(), child_gap) ;
  } else if (value_type == ValueType::BooleanObject) {
    Local<BooleanObject> real_value = Local<BooleanObject>::Cast(value) ;
    *result_ << (real_value->ValueOf() ? kJsonValueTrue : kJsonValueFalse) ;
  } else if (value_type == ValueType::NumberObject) {
    Local<NumberObject> real_value = Local<NumberObject>::Cast(value) ;
    *result_ << DoubleToUtf8(real_value->ValueOf()) ;
  } else if (value_type == ValueType::StringObject) {
    Local<StringObject> real_value = Local<StringObject>::Cast(value) ;
    SerializeValue(real_value->ValueOf(), child_gap) ;
  } else if (value_type == ValueType::SymbolObject) {
    Local<SymbolObject> real_value = Local<SymbolObject>::Cast(value) ;
    SerializeValue(real_value->ValueOf(), child_gap) ;
  } else {
    DCHECK(false) ;
    V8_LOG_ERR(errUnknown, "Unknown primitive object") ;
    *result_ << kJsonValueUndefined ;
  }

  // Serialize Object
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldObject[gap] ;
  SerializeObject(value, kEmptyId, child_gap) ;

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeProcessedValue(
    Local<Value> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, GetValueType(value), *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  *result_ << child_gap << kJsonFieldProcessed[gap] << kJsonValueTrue ;
  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializePromise(
    Local<Promise> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, ValueType::Promise, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  // Serialize flag of a handler existence
  *result_ << child_gap << kJsonFieldHasHandler[gap]
      << (value->HasHandler() ? kJsonValueTrue : kJsonValueFalse) ;

  // Serialize state
  static std::map<Promise::PromiseState, const char*> promise_state_map = {
    { Promise::kPending, R"("Pending")" },
    { Promise::kFulfilled, R"("Fulfilled")" },
    { Promise::kRejected, R"("Rejected")" },
  } ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldState[gap]
      << promise_state_map[value->State()] ;

  // Serialize result if the Promise isn't pending
  if (value->State() != Promise::kPending) {
    *result_ << kJsonComma[gap] << child_gap << kJsonFieldResult[gap] ;
    SerializeValue(value->Result(), child_gap) ;
  }

  // Serialize Object
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldObject[gap] ;
  SerializeObject(value, kEmptyId, child_gap) ;

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeRegExp(
    Local<RegExp> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, ValueType::RegExp, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  // Serialize source
  *result_ << child_gap << kJsonFieldSource[gap]
      << JSON_STRING(ValueToUtf8(context_, value->GetSource())) ;

  // Serialize flags
  static const char* flag_str_array[] = {
    "\"Global\"", "\"IgnoreCase\"", "\"Multiline\"", "\"Sticky\"",
    "\"Unicode\"", "\"DotAll\"" } ;
  RegExp::Flags flags = value->GetFlags() ;
  // Check that we know all flags
  DCHECK_LE(flags , 0x3f) ;

  *result_ << kJsonComma[gap] << child_gap << kJsonFieldFlags[gap] ;
  std::string flags_str ;
  if (!flags) {
    *result_ << kJsonEmptyArray[gap] ;
  } else {
    JsonGap flag_gap(child_gap) ;
    *result_ << kJsonLeftSquareBracket[gap] ;
    bool comma = false ;
    for (int i = 0; i < 6; ++i) {
      if (!((1 << i) & flags)) {
        continue ;
      }

      if (comma) {
        *result_ << kJsonComma[gap] ;
      } else {
        comma = true ;
      }

      *result_ << flag_gap << flag_str_array[i] ;
    }

    *result_ << kJsonNewLine[gap] << child_gap << kJsonRightSquareBracket[gap] ;
  }

  // Serialize Object
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldObject[gap] ;
  SerializeObject(value, kEmptyId, child_gap) ;

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeSet(
    Local<Set> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, ValueType::Set, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  *result_ << child_gap << kJsonFieldSize[gap] << value->Size() ;
  *result_ << kJsonComma[gap] ;

  // Serialize content
  *result_ << child_gap << kJsonFieldValue[gap] ;
  Local<Array> set = value->AsArray() ;
  SerializeValueArray(set, child_gap) ;

  // Serialize Object
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldObject[gap] ;
  SerializeObject(value, kEmptyId, child_gap) ;

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeSetIterator(
    Local<Object> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, ValueType::SetIterator, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  // Serialize value
  bool is_key_value = true ;
  *result_ << child_gap << kJsonFieldValue[gap] ;
  TryCatch try_catch(context_->GetIsolate()) ;
  Local<Array> iterator_value ;
  if (!value->PreviewEntries(&is_key_value).ToLocal(&iterator_value)) {
    SerializeException(try_catch, child_gap) ;
  } else if (iterator_value.IsEmpty() && iterator_value->Length() == 0) {
    *result_ << kJsonValueNull ;
  } else if (is_key_value) {
    *result_ << kJsonValueInvalid ;
  } else {
    // Serialize value
    v8::Local<v8::Value> item_value ;
    if (!iterator_value->Get(context_, 0).ToLocal(&item_value)) {
      SerializeException(try_catch, child_gap) ;
    } else {
      SerializeValue(item_value, child_gap) ;
    }
  }

  // Serialize Object
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldObject[gap] ;
  SerializeObject(value, kEmptyId, child_gap) ;

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeString(
    Local<String> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, ValueType::String, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  *result_ << child_gap << kJsonFieldValue[gap]
      << JSON_STRING(ValueToUtf8(context_, value)) ;
  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeSymbol(
    Local<Symbol> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, ValueType::Symbol, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  *result_ << child_gap << kJsonFieldValue[gap]
      << JSON_STRING(ValueToUtf8(context_, value->Name())) ;
  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeTypedArray(
    Local<TypedArray> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, ValueType::TypedArray, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  // Serialize value
  *result_ << child_gap << kJsonFieldLength[gap] << value->Length() ;

  // Serialize ArrayBufferView
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldArrayBufferView[gap] ;
  SerializeArrayBufferView(value, kEmptyId, child_gap) ;

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeTypedArrayObject(
    Local<TypedArray> value, uint64_t id, const JsonGap& gap) {
  ValueType value_type = GetValueType(value) ;
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, value_type, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  // Serialize value
  *result_ << child_gap << kJsonFieldValue[gap] ;
  Local<ArrayBuffer> buffer = value->Buffer() ;
  if (!value->HasBuffer() || buffer.IsEmpty() || !value->Length()) {
    *result_ << kJsonEmptyArray[gap] ;
  } else {
    JsonGap item_gap(child_gap) ;
    void* value_array =
        static_cast<char*>(buffer->GetContents().Data()) + value->ByteOffset() ;
    *result_ << kJsonLeftSquareBracket[gap] ;
    bool comma = false ;
    for (size_t i = 0, size = value->Length(); i < size; ++i) {
      if (comma) {
        *result_ << kJsonComma[gap] ;
      } else {
        comma = true ;
      }

      // TODO: Use "string-number-conversions.h": Uint8ToString(...) ...
      switch (value_type) {
        case ValueType::Uint8Array:
        case ValueType::Uint8ClampedArray:
          *result_ << item_gap
              << static_cast<uint64_t>(static_cast<uint8_t*>(value_array)[i]) ;
          break ;
        case ValueType::Int8Array:
          *result_ << item_gap
              << static_cast<int64_t>(static_cast<int8_t*>(value_array)[i]) ;
          break ;
        case ValueType::Uint16Array:
          *result_ << item_gap
              << static_cast<uint64_t>(static_cast<uint16_t*>(value_array)[i]) ;
          break ;
        case ValueType::Int16Array:
          *result_ << item_gap
              << static_cast<int64_t>(static_cast<int16_t*>(value_array)[i]) ;
          break ;
        case ValueType::Uint32Array:
          *result_ << item_gap
              << static_cast<uint64_t>(static_cast<uint32_t*>(value_array)[i]) ;
          break ;
        case ValueType::Int32Array:
          *result_ << item_gap
              << static_cast<int64_t>(static_cast<int32_t*>(value_array)[i]) ;
            break ;
        case ValueType::Float32Array:
          *result_ << item_gap
              << DoubleToUtf8(static_cast<float*>(value_array)[i]) ;
          break ;
        case ValueType::Float64Array:
          *result_ << item_gap
              << DoubleToUtf8(static_cast<double*>(value_array)[i]) ;
          break ;
        case ValueType::BigInt64Array:
          *result_ << item_gap << static_cast<int64_t*>(value_array)[i] ;
          break ;
        case ValueType::BigUint64Array:
          *result_ << item_gap << static_cast<int64_t*>(value_array)[i] ;
            break ;
        default:
          V8_LOG_ERR(
              errUnknown,
              "Invalid TypedArray type - %s", ValueTypeToUtf8(value_type)) ;
          *result_ << item_gap << static_cast<uint64_t>(0xdddddddd) ;
          break ;
      }
    }

    *result_ << kJsonNewLine[gap] << child_gap << kJsonRightSquareBracket[gap] ;
  }

  // Serialize TypedArray
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldTypedArray[gap] ;
  SerializeTypedArray(value, kEmptyId, child_gap) ;

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeWeakMap(
    Local<Object> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, ValueType::WeakMap, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  TryCatch try_catch(context_->GetIsolate()) ;

  // Try to get content
  Local<Array> map ;
  bool is_key_value = false ;
  if (!value->PreviewEntries(&is_key_value).ToLocal(&map)) {
    *result_ << child_gap << kJsonFieldValue[gap] ;
    SerializeException(try_catch, child_gap) ;
  } else {
    *result_ << child_gap << kJsonFieldSize[gap] << (map->Length() / 2) ;
    *result_ << kJsonComma[gap] ;
    *result_ << child_gap << kJsonFieldValue[gap] ;
    SerializeKeyValueArray(map, child_gap) ;
  }

  // Serialize Object
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldObject[gap] ;
  SerializeObject(value, kEmptyId, child_gap) ;

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeWeakSet(
    Local<Object> value, uint64_t id, const JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (SerializeCommonFileds(id, ValueType::WeakSet, *value, child_gap)) {
    *result_ << kJsonComma[gap] ;
  }

  TryCatch try_catch(context_->GetIsolate()) ;

  // Try to get content
  Local<Array> set ;
  bool is_key_value = false ;
  if (!value->PreviewEntries(&is_key_value).ToLocal(&set) &&
      try_catch.HasCaught()) {
    *result_ << child_gap << kJsonFieldValue[gap] ;
    SerializeException(try_catch, child_gap) ;
  } else {
    *result_ << child_gap << kJsonFieldSize[gap] << set->Length() ;
    *result_ << kJsonComma[gap] ;
    *result_ << child_gap << kJsonFieldValue[gap] ;
    SerializeValueArray(set, child_gap) ;
  }

  // Serialize Object
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldObject[gap] ;
  SerializeObject(value, kEmptyId, child_gap) ;

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

bool ValueSerializer::SerializeCommonFileds(
    uint64_t id, ValueType type, Value* value, const JsonGap& gap) {
  // Add id
  if (id != kEmptyId) {
    *result_ << gap << kJsonFieldId[gap] << id << kJsonComma[gap] ;
  }

  // Add type
  *result_ << gap << kJsonFieldType[gap]
      << JSON_STRING(ValueTypeToUtf8(type)) ;

  // Add flag of native error
  if (value && value->IsNativeError()) {
    *result_ << kJsonComma[gap] << gap << kJsonFieldNativeError[gap]
        << kJsonValueTrue ;
  }

  return true ;
}

void ValueSerializer::SerializeException(
    TryCatch& try_catch, const JsonGap& gap) {
  if (!try_catch.HasCaught()) {
    *result_ << kJsonValueException ;
  }

  *result_ << JSON_STRING(
      ("Exception[" + ValueToUtf8(context_, try_catch.Exception()) + "]")) ;
}

void ValueSerializer::SerializeKeyValueArray(
    Local<Array> value, const JsonGap& gap) {
  DCHECK(!(value->Length() & 1)) ;
  V8_LOG_ERR_WITH_FLAG(
      value->Length() & 1, errInvalidArgument, "Array size is odd - %d",
      value->Length()) ;

  if (!value->Length()) {
    *result_ << kJsonEmptyArray[gap] ;
    return ;
  }

  JsonGap item_gap(gap) ;
  JsonGap key_value_gap(item_gap) ;
  *result_ << kJsonLeftSquareBracket[gap] ;
  bool comma = false ;
  for (uint32_t i = 0, size = value->Length() - 1; i < size; i += 2) {
    if (comma) {
      *result_ << kJsonComma[gap] ;
    } else {
      comma = true ;
    }

    *result_ << item_gap << kJsonLeftBracket[gap] ;

    TryCatch try_catch(context_->GetIsolate()) ;

    // Serialize key
    *result_ << key_value_gap << kJsonFieldKey[gap] ;
    v8::Local<v8::Value> key_value ;
    if (!value->Get(context_, i).ToLocal(&key_value)) {
      SerializeException(try_catch, key_value_gap) ;
    } else {
      SerializeValue(key_value, key_value_gap) ;
    }

    *result_ << kJsonComma[gap] ;

    // Serialize value
    *result_ << key_value_gap << kJsonFieldValue[gap] ;
    v8::Local<v8::Value> value_value ;
    if (!value->Get(context_, i + 1).ToLocal(&value_value)) {
      SerializeException(try_catch, key_value_gap) ;
    } else {
      SerializeValue(value_value, key_value_gap) ;
    }

    *result_ << kJsonNewLine[gap] << item_gap << kJsonRightBracket[gap] ;
  }

  *result_ << kJsonNewLine[gap] << gap << kJsonRightSquareBracket[gap] ;
}

void ValueSerializer::SerializeValueArray(
    Local<Array> value, const JsonGap& gap) {
  if (!value->Length()) {
    *result_ << kJsonEmptyArray[gap] ;
    return ;
  }

  JsonGap item_gap(gap) ;
  *result_ << kJsonLeftSquareBracket[gap] ;
  bool comma = false ;
  for (uint32_t i = 0, size = value->Length(); i < size; ++i) {
    if (comma) {
      *result_ << kJsonComma[gap] ;
    } else {
      comma = true ;
    }

    *result_ << item_gap ;

    TryCatch try_catch(context_->GetIsolate()) ;
    v8::Local<v8::Value> array_value ;
    if (!value->Get(context_, i).ToLocal(&array_value)) {
      SerializeException(try_catch, item_gap) ;
      continue ;
    }

    SerializeValue(array_value, item_gap) ;
  }

  *result_ << kJsonNewLine[gap] << gap << kJsonRightSquareBracket[gap] ;
}

std::string ValueSerializer::ValueToField(
    Local<Value> value, const JsonGap& gap) {
  ValueType value_type = GetValueType(value) ;
  if (value_type == ValueType::String || value_type & ValueType::NumberTypes) {
    return JSON_FIELD(ValueToUtf8(context_, value), gap) ;
  } else if (value_type == ValueType::Symbol) {
    return JSON_SYMBOL_FIELD(
        ValueToUtf8(context_, Local<Symbol>::Cast(value)->Name()), gap) ;
  }

  V8_LOG_ERR(
      errInvalidArgument, "Don't have a serializer for a field type \'%s\'",
      ValueTypeToUtf8(value_type)) ;
  return kJsonFieldUndefinedType[gap] ;
}

const char* EdgeTypeToUtf8(HeapGraphEdge::Type edge_type) {
  switch (edge_type) {
    case HeapGraphEdge::kContextVariable: return kEdgeTypeContextVariable ;
    case HeapGraphEdge::kElement: return kEdgeTypeElement ;
    case HeapGraphEdge::kProperty: return kEdgeTypeProperty ;
    case HeapGraphEdge::kInternal: return kEdgeTypeInternal ;
    case HeapGraphEdge::kHidden: return kEdgeTypeHidden ;
    case HeapGraphEdge::kShortcut: return kEdgeTypeShortcut ;
    case HeapGraphEdge::kWeak: return kEdgeTypeWeak ;
  }

  DCHECK(false) ;
  V8_LOG_ERR(errUnknown, "Unknown graph edge type - %d", edge_type) ;
  return kEdgeTypeUnknown ;
}

const char* NodeTypeToUtf8(HeapGraphNode::Type node_type) {
  switch (node_type) {
    case HeapGraphNode::kArray: return kNodeTypeArray ;
    case HeapGraphNode::kBigInt: return kNodeTypeBigInt ;
    case HeapGraphNode::kClosure: return kNodeTypeClosure ;
    case HeapGraphNode::kCode: return kNodeTypeCode ;
    case HeapGraphNode::kConsString: return kNodeTypeConsString ;
    case HeapGraphNode::kHeapNumber: return kNodeTypeHeapNumber ;
    case HeapGraphNode::kHidden: return kNodeTypeHidden ;
    case HeapGraphNode::kNative: return kNodeTypeNative ;
    case HeapGraphNode::kObject: return kNodeTypeObject ;
    case HeapGraphNode::kRegExp: return kNodeTypeRegExp ;
    case HeapGraphNode::kSlicedString: return kNodeTypeSlicedString ;
    case HeapGraphNode::kString: return kNodeTypeString ;
    case HeapGraphNode::kSynthetic: return kNodeTypeSynthetic ;
    case HeapGraphNode::kSymbol: return kNodeTypeSymbol ;
  }

  DCHECK(false) ;
  V8_LOG_ERR(errUnknown, "Unknown graph node type - %d", node_type) ;
  return kNodeTypeUnknown ;
}

void SerializeHeapNode(
    Isolate* isolate, HeapProfiler* heap_profile, const HeapGraphNode* node,
    ProcessedNodeArray& processed_nodes, const JsonGap& gap,
    std::ostream& result) ;

void SerializeHeapEdge(
    Isolate* isolate, HeapProfiler* heap_profile, const HeapGraphEdge* edge,
    ProcessedNodeArray& processed_nodes, const JsonGap& gap,
    std::ostream& result) {
  JsonGap child_gap(gap) ;
  result << kJsonLeftBracket[gap] ;
  result << child_gap << kJsonFieldType[gap]
      << EdgeTypeToUtf8(edge->GetType()) ;
  std::string name = ValueToUtf8(isolate, edge->GetName()) ;
  if (name.length()) {
    result << kJsonComma[gap] ;
    if (edge->GetType() == HeapGraphEdge::kElement) {
      result << child_gap << kJsonFieldIndex[gap] << name ;
    } else {
      result << child_gap << kJsonFieldName[gap]
          << "\"" << name << "\"" ;
    }
  }

  const HeapGraphNode* node = edge->GetToNode() ;
  if (node) {
    result << kJsonComma[gap] << child_gap
        << kJsonFieldNode[gap] ;
    SerializeHeapNode(
        isolate, heap_profile, node, processed_nodes, child_gap, result) ;
  }

  result << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void SerializeHeapNode(
    Isolate* isolate, HeapProfiler* heap_profile, const HeapGraphNode* node,
    ProcessedNodeArray& processed_nodes, const JsonGap& gap,
    std::ostream& result) {
  JsonGap child_gap(gap) ;
  result << kJsonLeftBracket[gap] ;
  result << child_gap << kJsonFieldId[gap] << node->GetId() ;
  result << kJsonComma[gap] ;
  result << child_gap << kJsonFieldType[gap]
      << NodeTypeToUtf8(node->GetType()) ;
  // If we've alredy processed the node
  // then we'll write a flag and cancel the branch
  if (processed_nodes.find(node->GetId()) != processed_nodes.end()) {
    result << kJsonComma[gap] ;
    result << child_gap << kJsonFieldProcessed[gap] << kJsonValueTrue ;
    result << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
    return ;
  }

  // Add Id to processed nodes
  processed_nodes.insert(node->GetId()) ;

  // Add a name or a value
  std::string name = ValueToUtf8(isolate, node->GetName()) ;
  if (name.length()) {
    result << kJsonComma[gap] ;
    if (node->GetType() == HeapGraphNode::kString){
      result << child_gap << kJsonFieldValue[gap] ;
    } else {
      result << child_gap << kJsonFieldName[gap] ;
    }

    result  << JSON_STRING(name) ;
  }

  if (node->GetType() == HeapGraphNode::kBigInt ||
      node->GetType() == HeapGraphNode::kHeapNumber) {
    Local<Value> value = heap_profile->FindObjectById(node->GetId()) ;
    if (!value.IsEmpty()) {
      result << kJsonComma[gap] ;
      result << child_gap << kJsonFieldValue[gap] ;
      std::string value_str = ValueToUtf8(isolate, value) ;
      if (IsNumber(value_str.c_str())) {
        result << value_str ;
      } else {
        result  << JSON_STRING(value_str) ;
      }
    }
  }

  // Write a child edges
  result << kJsonComma[gap] << child_gap << kJsonFieldEdges[gap] ;
  if (node->GetChildrenCount()) {
    result << kJsonLeftSquareBracket[gap] ;
    bool comma = false ;
    JsonGap edge_gap(child_gap) ;
    for (int index = 0, size = node->GetChildrenCount();
         index < size; ++index) {
      const HeapGraphEdge* child_edge = node->GetChild(index) ;
      if (!child_edge) {
        continue ;
      }

      if (comma) {
        result << kJsonComma[gap] ;
      } else {
        comma = true ;
      }

      result << edge_gap ;
      SerializeHeapEdge(
          isolate, heap_profile, child_edge, processed_nodes,
          edge_gap, result) ;
    }

    result << kJsonNewLine[gap] << child_gap << kJsonRightSquareBracket[gap] ;
  } else {
    result << kJsonEmptyArray[gap] ;
  }

  result << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

}  // namespace

Error CreateContextDump(
    Local<Context> context, std::ostream& result,
    FormattedJson formatted /*= FormattedJson::False*/) {
  V8_LOG_FUNCTION_BODY() ;

  ValueSerializer serializer(context, formatted) ;
  serializer.Serialize(context->Global(), result) ;
  return errOk ;
}

Error CreateHeapDump(Isolate* isolate, std::ostream& result) {
  V8_LOG_FUNCTION_BODY() ;

  HeapProfiler* heap_profile = isolate->GetHeapProfiler() ;
  const HeapSnapshot* heap_shapshort = heap_profile->TakeHeapSnapshot() ;
  if (heap_shapshort) {
    HeapSerializer serializer(result) ;
    heap_shapshort->Serialize(&serializer, HeapSnapshot::kJSON) ;
  }

  return errOk ;
}

Error CreateHeapGraphDump(
    Isolate* isolate, std::ostream& result,
    FormattedJson formatted /*= FormattedJson::False*/) {
  V8_LOG_FUNCTION_BODY() ;

  HeapProfiler* heap_profile = isolate->GetHeapProfiler() ;
  const HeapSnapshot* heap_shapshort = heap_profile->TakeHeapSnapshot() ;
  if (heap_shapshort) {
    JsonGap root_gap(formatted, 0) ;
    JsonGap child_gap(root_gap) ;
    result << kJsonLeftBracket[root_gap] ;
    result << child_gap << kJsonFieldNodeCount[root_gap]
        << heap_shapshort->GetNodesCount() ;
    const HeapGraphNode* root = heap_shapshort->GetRoot() ;
    if (root) {
      result << kJsonComma[root_gap] ;
      result << child_gap << kJsonFieldNodes[root_gap] ;
      ProcessedNodeArray processed_nodes ;
      SerializeHeapNode(
          isolate, heap_profile, root, processed_nodes, child_gap,
          result) ;
    }

    result << kJsonNewLine[root_gap] << kJsonRightBracket[root_gap] ;
  }

  return errOk ;
}

}  // namespace internal
}  // namespace vm
}  // namespace v8
