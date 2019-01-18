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
const char* kJsonFieldConstructorName[] =
    JSON_ARRAY_OF_FIELD(constructor_name) ;
const char* kJsonFieldDebugName[] = JSON_ARRAY_OF_FIELD(debug_name) ;
const char* kJsonFieldDisplayName[] = JSON_ARRAY_OF_FIELD(display_name) ;
const char* kJsonFieldEdges[] = JSON_ARRAY_OF_FIELD(edges) ;
const char* kJsonFieldId[] = JSON_ARRAY_OF_FIELD(id) ;
const char* kJsonFieldIndex[] = JSON_ARRAY_OF_FIELD(index) ;
const char* kJsonFieldInferredName[] = JSON_ARRAY_OF_FIELD(inferred_name) ;
const char* kJsonFieldInternalFieldCount[] =
    JSON_ARRAY_OF_FIELD(internal_field_count) ;
const char* kJsonFieldInternalFields[] = JSON_ARRAY_OF_FIELD(internal_fields) ;
const char* kJsonFieldLength[] = JSON_ARRAY_OF_FIELD(length) ;
const char* kJsonFieldName[] = JSON_ARRAY_OF_FIELD(name) ;
const char* kJsonFieldNode[] = JSON_ARRAY_OF_FIELD(node) ;
const char* kJsonFieldNodeCount[] = JSON_ARRAY_OF_FIELD(node_count) ;
const char* kJsonFieldNodes[] = JSON_ARRAY_OF_FIELD(nodes) ;
const char* kJsonFieldObject[] = JSON_ARRAY_OF_FIELD(__object__) ;
const char* kJsonFieldProcessed[] = JSON_ARRAY_OF_FIELD(processed) ;
const char* kJsonFieldProperties[] = JSON_ARRAY_OF_FIELD(properties) ;
const char* kJsonFieldPropertyCount[] = JSON_ARRAY_OF_FIELD(property_count) ;
const char* kJsonFieldProto[] = JSON_ARRAY_OF_FIELD(__proto__) ;
const char* kJsonFieldResourceName[] = JSON_ARRAY_OF_FIELD(resource_name) ;
const char* kJsonFieldScriptLine[] = JSON_ARRAY_OF_FIELD(script_line) ;
const char* kJsonFieldScriptColumn[] = JSON_ARRAY_OF_FIELD(script_column) ;
const char* kJsonFieldSize[] = JSON_ARRAY_OF_FIELD(size) ;
const char* kJsonFieldToString[] = JSON_ARRAY_OF_FIELD(to_string) ;
const char* kJsonFieldType[] = JSON_ARRAY_OF_FIELD(type) ;
const char* kJsonFieldUndefinedType[] =
    JSON_ARRAY_OF_FIELD([undefined field type]) ;
const char* kJsonFieldValue[] = JSON_ARRAY_OF_FIELD(value) ;

// Json value list
const char kJsonValueException[] = R"("[exception]")" ;
const char kJsonValueFalse[] = "false" ;
const char kJsonValueInfinity[] = R"("Infinity")" ;
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
  void SerializeArray(Local<Array> value, uint64_t id, JsonGap& gap) ;
  void SerializeBigInt(Local<BigInt> value, uint64_t id, JsonGap& gap) ;
  void SerializeBoolean(Local<Boolean> value, uint64_t id, JsonGap& gap) ;
  void SerializeException(TryCatch& try_catch, JsonGap& gap) ;
  void SerializeFunction(Local<Function> value, uint64_t id, JsonGap& gap) ;
  void SerializeKeyValueArray(Local<Array> value, JsonGap& gap) ;
  void SerializeMap(Local<Map> value, uint64_t id, JsonGap& gap) ;
  void SerializeNumber(Local<Number> value, uint64_t id, JsonGap& gap) ;
  void SerializeObject(Local<Object> value, uint64_t id, JsonGap& gap) ;
  void SerializePrimitiveObject(
      Local<Object> value, uint64_t id, JsonGap& gap) ;
  void SerializeProcessedValue(Local<Value> value, uint64_t id, JsonGap& gap) ;
  void SerializeSet(Local<Set> value, uint64_t id, JsonGap& gap) ;
  void SerializeString(Local<String> value, uint64_t id, JsonGap& gap) ;
  void SerializeSymbol(Local<Symbol> value, uint64_t id, JsonGap& gap) ;
  void SerializeValue(Local<Value> value, JsonGap& gap) ;
  void SerializeValueArray(Local<Array> value, JsonGap& gap) ;
  void SerializeWeakMap(Local<Object> value, uint64_t id, JsonGap& gap) ;
  void SerializeWeakSet(Local<Object> value, uint64_t id, JsonGap& gap) ;

  std::string ValueToField(Local<Value> value, JsonGap& gap) ;

  Local<Context> context_ ;
  JsonGapArray gaps_ ;
  JsonGap default_gap_ ;
  std::map<i::Object*, uint64_t> processed_values_ ;
  std::ostream* result_ = nullptr ;

  static const uint64_t kEmptyId ;

  DISALLOW_COPY_AND_ASSIGN(ValueSerializer) ;
};

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

const uint64_t ValueSerializer::kEmptyId = static_cast<uint64_t>(-1) ;

ValueSerializer::ValueSerializer(
    Local<Context> context, FormattedJson default_format)
  : context_(context), default_gap_(gaps_, default_format, 0) {}

void ValueSerializer::Serialize(
    Local<Value> value, std::ostream& result, JsonGap* gap /*= nullptr*/) {
  if (!gap) {
    gap = &default_gap_ ;
  }

  result_ = &result ;
  SerializeValue(value, *gap) ;
  result_ = nullptr ;
}

void ValueSerializer::SerializeArray(
    Local<Array> value, uint64_t id, JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  *result_ << child_gap << kJsonFieldId[gap] << id ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldType[gap]
      << JSON_STRING(ValueTypeToUtf8(ValueType::Array)) ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldLength[gap]
      << value->Length() ;

  // Serialize a array content
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldValue[gap] ;
  SerializeValueArray(value, child_gap) ;

  // Serialize Object
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldObject[gap] ;
  SerializeObject(value, kEmptyId, child_gap) ;

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeBigInt(
    Local<BigInt> value, uint64_t id, JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  *result_ << child_gap << kJsonFieldId[gap] << id ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldType[gap]
      << JSON_STRING(ValueTypeToUtf8(ValueType::BigInt)) ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldValue[gap] ;
  std::string value_str = ValueToUtf8(context_, value) ;
  if (IsNumber(value_str.c_str())) {
    *result_ << value_str ;
  } else {
    *result_  << JSON_STRING(value_str) ;
  }

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeBoolean(
    Local<Boolean> value, uint64_t id, JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  *result_ << child_gap << kJsonFieldId[gap] << id ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldType[gap]
      << JSON_STRING(ValueTypeToUtf8(ValueType::Boolean)) ;
  *result_ << kJsonComma[gap] << child_gap ;
  *result_ << kJsonFieldValue[gap]
      << (value->Value() ? kJsonValueTrue : kJsonValueFalse) ;
  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeException(TryCatch& try_catch, JsonGap& gap) {
  if (!try_catch.HasCaught()) {
    *result_ << kJsonValueException ;
  }

  *result_ << JSON_STRING(
      ("Exception[" + ValueToUtf8(context_, try_catch.Exception()) + "]")) ;
}

void ValueSerializer::SerializeFunction(
    Local<Function> value, uint64_t id, JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  *result_ << child_gap << kJsonFieldId[gap] << id ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldType[gap]
      << JSON_STRING(ValueTypeToUtf8(ValueType::Function)) ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldToString[gap]
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

void ValueSerializer::SerializeKeyValueArray(Local<Array> value, JsonGap& gap) {
  if (!value->Length()) {
    *result_ << kJsonEmptyArray[gap] ;
    return ;
  }

  DCHECK(!(value->Length() & 1)) ;

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

    *result_ << item_gap << kJsonLeftSquareBracket[gap] ;

    TryCatch try_catch(context_->GetIsolate()) ;

    // Serialize key
    *result_ << key_value_gap ;
    v8::Local<v8::Value> key_value ;
    if (!value->Get(context_, i).ToLocal(&key_value)) {
      SerializeException(try_catch, key_value_gap) ;
    } else {
      SerializeValue(key_value, key_value_gap) ;
    }

    *result_ << kJsonComma[gap] ;

    // Serialize value
    *result_ << key_value_gap ;
    v8::Local<v8::Value> value_value ;
    if (!value->Get(context_, i + 1).ToLocal(&value_value)) {
      SerializeException(try_catch, key_value_gap) ;
    } else {
      SerializeValue(value_value, key_value_gap) ;
    }

    *result_ << kJsonNewLine[gap] << item_gap << kJsonRightSquareBracket[gap] ;
  }

  *result_ << kJsonNewLine[gap] << gap << kJsonRightSquareBracket[gap] ;
}

void ValueSerializer::SerializeMap(
    Local<Map> value, uint64_t id, JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  *result_ << child_gap << kJsonFieldId[gap] << id ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldType[gap]
      << JSON_STRING(ValueTypeToUtf8(ValueType::Map)) ;
  *result_ << kJsonComma[gap] ;
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

void ValueSerializer::SerializeNumber(
    Local<Number> value, uint64_t id, JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  *result_ << child_gap << kJsonFieldId[gap] << id ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldType[gap]
      << JSON_STRING(ValueTypeToUtf8(GetValueType(value))) ;
  *result_ << kJsonComma[gap] ;
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
    Local<Object> value, uint64_t id, JsonGap& gap) {
  JsonGap child_gap(gap) ;
  JsonGap array_gap(child_gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  if (id != kEmptyId) {
    *result_ << child_gap << kJsonFieldId[gap] << id << kJsonComma[gap] ;
  }

  *result_ << child_gap << kJsonFieldType[gap]
      << JSON_STRING(ValueTypeToUtf8(ValueType::Object)) ;
  *result_ << kJsonComma[gap] ;
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
    Local<Object> value, uint64_t id, JsonGap& gap) {
  ValueType value_type = GetValueType(value) ;
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  *result_ << child_gap << kJsonFieldId[gap] << id ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldType[gap]
      << JSON_STRING(ValueTypeToUtf8(value_type)) ;
  *result_ << kJsonComma[gap] << child_gap ;
  *result_ << kJsonFieldValue[gap] ;

  // Serialize value
  if (value_type == ValueType::BigIntObject) {
    Local<BigIntObject> real_value = Local<BigIntObject>::Cast(value) ;
    SerializeValue(real_value->ValueOf(), child_gap) ;
  } else if (value_type == ValueType::BooleanObject) {
    Local<BooleanObject> real_value = Local<BooleanObject>::Cast(value) ;
    *result_ << (real_value->ValueOf() ? kJsonValueTrue : kJsonValueFalse) ;
  } else if (value_type == ValueType::NumberObject) {
    Local<NumberObject> real_value = Local<NumberObject>::Cast(value) ;
    double number_value = real_value->ValueOf() ;
    if (std::isnan(number_value)) {
      *result_ << kJsonValueNaN ;
    } else if (std::isinf(number_value)) {
      *result_ << (number_value == std::numeric_limits<double>::infinity() ?
                   kJsonValueInfinity : kJsonValueNegativeInfinity) ;
    } else {
      // TODO: Use "string-number-conversions.h": DoubleToString(...)
      *result_ << std::to_string(number_value) ;
    }
  } else if (value_type == ValueType::StringObject) {
    Local<StringObject> real_value = Local<StringObject>::Cast(value) ;
    SerializeValue(real_value->ValueOf(), child_gap) ;
  } else if (value_type == ValueType::SymbolObject) {
    Local<SymbolObject> real_value = Local<SymbolObject>::Cast(value) ;
    SerializeValue(real_value->ValueOf(), child_gap) ;
  } else {
    DCHECK(false) ;
    *result_ << kJsonValueUndefined ;
  }

  // Serialize Object
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldObject[gap] ;
  SerializeObject(value, kEmptyId, child_gap) ;

  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeProcessedValue(
    Local<Value> value, uint64_t id, JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  *result_ << child_gap << kJsonFieldId[gap] << id ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldType[gap]
      << JSON_STRING(ValueTypeToUtf8(GetValueType(value))) ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldProcessed[gap]
      << kJsonValueTrue ;
  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeSet(
    Local<Set> value, uint64_t id, JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  *result_ << child_gap << kJsonFieldId[gap] << id ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldType[gap]
      << JSON_STRING(ValueTypeToUtf8(ValueType::Set)) ;
  *result_ << kJsonComma[gap] ;
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

void ValueSerializer::SerializeString(
    Local<String> value, uint64_t id, JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  *result_ << child_gap << kJsonFieldId[gap] << id ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldType[gap]
      << JSON_STRING(ValueTypeToUtf8(ValueType::String)) ;
  *result_ << kJsonComma[gap] << child_gap ;
  *result_ << kJsonFieldValue[gap]
      << JSON_STRING(ValueToUtf8(context_, value)) ;
  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeSymbol(
    Local<Symbol> value, uint64_t id, JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  *result_ << child_gap << kJsonFieldId[gap] << id ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldType[gap]
      << JSON_STRING(ValueTypeToUtf8(ValueType::Symbol)) ;
  *result_ << kJsonComma[gap] << child_gap ;
  *result_ << kJsonFieldValue[gap]
      << JSON_STRING(ValueToUtf8(context_, value->Name())) ;
  *result_ << kJsonNewLine[gap] << gap << kJsonRightBracket[gap] ;
}

void ValueSerializer::SerializeValue(Local<Value> value, JsonGap& gap) {
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
  } else if (value_type == ValueType::Array) {
    SerializeArray(Local<Array>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::BigInt) {
    SerializeBigInt(Local<BigInt>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::Boolean) {
    SerializeBoolean(Local<Boolean>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::Function) {
    SerializeFunction(Local<Function>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::Map) {
    SerializeMap(Local<Map>::Cast(value), id, gap) ;
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
  } else if (value_type == ValueType::Set) {
    SerializeSet(Local<Set>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::String) {
    SerializeString(Local<String>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::Symbol) {
    SerializeSymbol(Local<Symbol>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::WeakMap) {
    SerializeWeakMap(Local<Object>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::WeakSet) {
    SerializeWeakSet(Local<Object>::Cast(value), id, gap) ;
    return ;
  }

#ifdef DEBUG
  USE(kJsonValueUnknown) ;
  *result_ << JSON_STRING(
      "Don't have a serializer for \'" +
      std::string(ValueTypeToUtf8(value_type)) + "\'") ;
#else
  *result_ << kJsonValueUnknown ;
#endif  // DEBUG
}

void ValueSerializer::SerializeValueArray(Local<Array> value, JsonGap& gap) {
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

void ValueSerializer::SerializeWeakMap(
    Local<Object> value, uint64_t id, JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  *result_ << child_gap << kJsonFieldId[gap] << id ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldType[gap]
      << JSON_STRING(ValueTypeToUtf8(ValueType::WeakMap)) ;
  *result_ << kJsonComma[gap] ;

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
    Local<Object> value, uint64_t id, JsonGap& gap) {
  JsonGap child_gap(gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  *result_ << child_gap << kJsonFieldId[gap] << id ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldType[gap]
      << JSON_STRING(ValueTypeToUtf8(ValueType::WeakSet)) ;
  *result_ << kJsonComma[gap] ;

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

std::string ValueSerializer::ValueToField(Local<Value> value, JsonGap& gap) {
  ValueType value_type = GetValueType(value) ;
  if (value_type == ValueType::String || value_type & ValueType::NumberTypes) {
    return JSON_FIELD(ValueToUtf8(context_, value), gap) ;
  } else if (value_type == ValueType::Symbol) {
    return JSON_SYMBOL_FIELD(
        ValueToUtf8(context_, Local<Symbol>::Cast(value)->Name()), gap) ;
  }

#ifdef DEBUG
  USE(kJsonFieldUndefinedType) ;
  return JSON_FIELD(std::string(
      "Don't have a serializer for a field type \'" +
      std::string(ValueTypeToUtf8(value_type)) + "\'"), gap) ;
#else
  return kJsonFieldUndefinedType[gap] ;
#endif  // DEBUG
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
  return kNodeTypeUnknown ;
}

void SerializeHeapNode(
    Isolate* isolate, HeapProfiler* heap_profile, const HeapGraphNode* node,
    ProcessedNodeArray& processed_nodes, JsonGap& gap, std::ostream& result) ;

void SerializeHeapEdge(
    Isolate* isolate, HeapProfiler* heap_profile, const HeapGraphEdge* edge,
    ProcessedNodeArray& processed_nodes, JsonGap& gap, std::ostream& result) {
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
    ProcessedNodeArray& processed_nodes, JsonGap& gap, std::ostream& result) {
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

void CreateContextDump(
    Local<Context> context, std::ostream& result,
    FormattedJson formatted /*= FormattedJson::False*/) {
  ValueSerializer serializer(context, formatted) ;
  serializer.Serialize(context->Global(), result) ;
}

void CreateHeapDump(Isolate* isolate, std::ostream& result) {
  HeapProfiler* heap_profile = isolate->GetHeapProfiler() ;
  const HeapSnapshot* heap_shapshort = heap_profile->TakeHeapSnapshot() ;
  if (heap_shapshort) {
    HeapSerializer serializer(result) ;
    heap_shapshort->Serialize(&serializer, HeapSnapshot::kJSON) ;
  }
}

void CreateHeapGraphDump(
    Isolate* isolate, std::ostream& result,
    FormattedJson formatted /*= FormattedJson::False*/) {
  HeapProfiler* heap_profile = isolate->GetHeapProfiler() ;
  const HeapSnapshot* heap_shapshort = heap_profile->TakeHeapSnapshot() ;
  if (heap_shapshort) {
    JsonGapArray gaps ;
    JsonGap root_gap(gaps, formatted, 0) ;
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
}

}  // namespace internal
}  // namespace vm
}  // namespace v8
