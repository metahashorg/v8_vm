// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/vm/dumper.h"

#include <cmath>
#include <limits>

#include "src/api.h"
#include "src/vm/vm-utils.h"

// Json defines
#define JSON_ARRAY_OF_FIELD(name) \
    { "\"" #name "\":", "\"" #name "\": " }

#define JSON_FIELD(field, gap) \
    (("\"" + field + "\"") + ((int)gap ? ": " : ":"))

#define JSON_STRING(str) \
    ("\"" + EncodeJsonString(str) + "\"")

#define JSON_SYMBOL_FIELD(name, gap) \
    JSON_FIELD(("Symbol(" + name + ")"), gap)

namespace v8 {
namespace vm {
namespace internal {

namespace {

typedef std::vector<std::string> JsonGapArray ;
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
const char kJsonValueFalse[] = "false" ;
const char kJsonValueInfinity[] = R"("Infinity")" ;
const char kJsonValueNaN[] = R"("NaN")" ;
const char kJsonValueNegativeInfinity[] = R"("-Infinity")" ;
const char kJsonValueNull[] = "null" ;
const char kJsonValueTrue[] = "true" ;
const char kJsonValueUndefined[] = R"("[undefined]")" ;

// Json special symbol list
const char* kJsonComma[] = { ",", ",\n" } ;
const char* kJsonEmptyArray[] = { "[]", "[]" } ;
const char* kJsonGap[] = { "", "  " } ;
const char* kJsonLeftBracket[] = { "{", "{\n" } ;
const char* kJsonLeftSquareBracket[] = { "[", "[\n" } ;
const char* kJsonNewLine[] = { "", "\n" } ;
const char* kJsonRightBracket[] = { "}", "}" } ;
const char* kJsonRightSquareBracket[] = { "]", "]" } ;

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

// Translation table to escape Latin1 characters.
// Table entries start at a multiple of 8 and are null-terminated.
const char* const JsonEscapeTable =
    "\\u0000\0 \\u0001\0 \\u0002\0 \\u0003\0 "
    "\\u0004\0 \\u0005\0 \\u0006\0 \\u0007\0 "
    "\\b\0     \\t\0     \\n\0     \\u000b\0 "
    "\\f\0     \\r\0     \\u000e\0 \\u000f\0 "
    "\\u0010\0 \\u0011\0 \\u0012\0 \\u0013\0 "
    "\\u0014\0 \\u0015\0 \\u0016\0 \\u0017\0 "
    "\\u0018\0 \\u0019\0 \\u001a\0 \\u001b\0 "
    "\\u001c\0 \\u001d\0 \\u001e\0 \\u001f\0 "
    " \0      !\0      \\\"\0     #\0      "
    "$\0      %\0      &\0      '\0      "
    "(\0      )\0      *\0      +\0      "
    ",\0      -\0      .\0      /\0      "
    "0\0      1\0      2\0      3\0      "
    "4\0      5\0      6\0      7\0      "
    "8\0      9\0      :\0      ;\0      "
    "<\0      =\0      >\0      ?\0      "
    "@\0      A\0      B\0      C\0      "
    "D\0      E\0      F\0      G\0      "
    "H\0      I\0      J\0      K\0      "
    "L\0      M\0      N\0      O\0      "
    "P\0      Q\0      R\0      S\0      "
    "T\0      U\0      V\0      W\0      "
    "X\0      Y\0      Z\0      [\0      "
    "\\\\\0     ]\0      ^\0      _\0      "
    "`\0      a\0      b\0      c\0      "
    "d\0      e\0      f\0      g\0      "
    "h\0      i\0      j\0      k\0      "
    "l\0      m\0      n\0      o\0      "
    "p\0      q\0      r\0      s\0      "
    "t\0      u\0      v\0      w\0      "
    "x\0      y\0      z\0      {\0      "
    "|\0      }\0      ~\0      \x7F\0      "
    "\x80\0      \x81\0      \x82\0      \x83\0      "
    "\x84\0      \x85\0      \x86\0      \x87\0      "
    "\x88\0      \x89\0      \x8A\0      \x8B\0      "
    "\x8C\0      \x8D\0      \x8E\0      \x8F\0      "
    "\x90\0      \x91\0      \x92\0      \x93\0      "
    "\x94\0      \x95\0      \x96\0      \x97\0      "
    "\x98\0      \x99\0      \x9A\0      \x9B\0      "
    "\x9C\0      \x9D\0      \x9E\0      \x9F\0      "
    "\xA0\0      \xA1\0      \xA2\0      \xA3\0      "
    "\xA4\0      \xA5\0      \xA6\0      \xA7\0      "
    "\xA8\0      \xA9\0      \xAA\0      \xAB\0      "
    "\xAC\0      \xAD\0      \xAE\0      \xAF\0      "
    "\xB0\0      \xB1\0      \xB2\0      \xB3\0      "
    "\xB4\0      \xB5\0      \xB6\0      \xB7\0      "
    "\xB8\0      \xB9\0      \xBA\0      \xBB\0      "
    "\xBC\0      \xBD\0      \xBE\0      \xBF\0      "
    "\xC0\0      \xC1\0      \xC2\0      \xC3\0      "
    "\xC4\0      \xC5\0      \xC6\0      \xC7\0      "
    "\xC8\0      \xC9\0      \xCA\0      \xCB\0      "
    "\xCC\0      \xCD\0      \xCE\0      \xCF\0      "
    "\xD0\0      \xD1\0      \xD2\0      \xD3\0      "
    "\xD4\0      \xD5\0      \xD6\0      \xD7\0      "
    "\xD8\0      \xD9\0      \xDA\0      \xDB\0      "
    "\xDC\0      \xDD\0      \xDE\0      \xDF\0      "
    "\xE0\0      \xE1\0      \xE2\0      \xE3\0      "
    "\xE4\0      \xE5\0      \xE6\0      \xE7\0      "
    "\xE8\0      \xE9\0      \xEA\0      \xEB\0      "
    "\xEC\0      \xED\0      \xEE\0      \xEF\0      "
    "\xF0\0      \xF1\0      \xF2\0      \xF3\0      "
    "\xF4\0      \xF5\0      \xF6\0      \xF7\0      "
    "\xF8\0      \xF9\0      \xFA\0      \xFB\0      "
    "\xFC\0      \xFD\0      \xFE\0      \xFF\0      " ;

static const int kJsonEscapeTableEntrySize = 8 ;

// Enum of value types
enum class ValueType : uint64_t {
  // Unknown type of value(ECMA-262 4.3.10)
  Unknown                   = 0,
  // The null value (ECMA-262 4.3.11)
  Null                      = 1,
  // String type (ECMA-262 8.4)
  String                    = Null << 1,
  // Symbol
  Symbol                    = String << 1,
  // Function
  Function                  = Symbol << 1,
  // Array
  Array                     = Function << 1,
  // Object
  Object                    = Array << 1,
  // Bigint
  BigInt                    = Object << 1,
  // Boolean
  Boolean                   = BigInt << 1,
  // Number
  Number                    = Boolean << 1,
  // 32-bit signed integer
  Int32                     = Number << 1,
  // 32-bit unsigned integer
  Uint32                    = Int32 << 1,
  // Date
  Date                      = Uint32 << 1,
  // Arguments object
  ArgumentsObject           = Date << 1,
  // BigInt object
  BigIntObject              = ArgumentsObject << 1,
  // Boolean object
  BooleanObject             = BigIntObject << 1,
  // Number object
  NumberObject              = BooleanObject << 1,
  // String object
  StringObject              = NumberObject << 1,
  // Symbol object
  SymbolObject              = StringObject << 1,
  // NativeError
  NativeError               = SymbolObject << 1,
  // RegExp
  RegExp                    = NativeError << 1,
  // Async function
  AsyncFunction             = RegExp << 1,
  // Generator function
  GeneratorFunction         = AsyncFunction << 1,
  // Generator object (iterator)
  GeneratorObject           = GeneratorFunction << 1,
  // Promise
  Promise                   = GeneratorObject << 1,
  // Map
  Map                       = Promise << 1,
  // Set
  Set                       = Map << 1,
  // Map Iterator
  MapIterator               = Set << 1,
  // Set Iterator
  SetIterator               = MapIterator << 1,
  // WeakMap
  WeakMap                   = SetIterator << 1,
  // WeakSet
  WeakSet                   = WeakMap << 1,
  // ArrayBuffer
  ArrayBuffer               = WeakSet << 1,
  // ArrayBufferView
  ArrayBufferView           = ArrayBuffer << 1,
  // Uint8Array
  Uint8Array                = ArrayBufferView << 1,
  // Uint8ClampedArray
  Uint8ClampedArray         = Uint8Array << 1,
  // Int8Array
  Int8Array                 = Uint8ClampedArray << 1,
  // Uint16Array
  Uint16Array               = Int8Array << 1,
  // Int16Array
  Int16Array                = Uint16Array << 1,
  // Uint32Array
  Uint32Array               = Int16Array << 1,
  // Int32Array
  Int32Array                = Uint32Array << 1,
  // Float32Array
  Float32Array              = Int32Array << 1,
  // Float64Array
  Float64Array              = Float32Array << 1,
  // BigInt64Array
  BigInt64Array             = Float64Array << 1,
  // BigUint64Array
  BigUint64Array            = BigInt64Array << 1,
  // DataView
  DataView                  = BigUint64Array << 1,
  // SharedArrayBuffer (This is an experimental feature)
  SharedArrayBuffer         = DataView << 1,
  // JavaScript Proxy
  Proxy                     = SharedArrayBuffer << 1,
  // WebAssembly Compiled Module
  WebAssemblyCompiledModule = Proxy << 1,
  // Module Namespace Object
  ModuleNamespaceObject     = WebAssemblyCompiledModule << 1,

  // Array types
  //Arrays = ;

  // Number types
  NumberTypes = Number | Int32 | Uint32,

  // Primitive object types
  PrimitiveObjectTypes = BigIntObject | BooleanObject | NumberObject |
                         StringObject | SymbolObject,
};

ValueType GetValueType(Value* value) {
  # define CHECK_TYPE_AND_RETURN(type) \
      if (value->Is##type()) { return ValueType::type ; }

  if (!value) {
    DCHECK(false) ;
    return ValueType::Unknown ;
  }

  if (value->IsUndefined()) {
    return ValueType::Unknown ;
  }

  CHECK_TYPE_AND_RETURN(Null) ;
  CHECK_TYPE_AND_RETURN(String) ;
  CHECK_TYPE_AND_RETURN(Symbol) ;
  CHECK_TYPE_AND_RETURN(Function) ;
  CHECK_TYPE_AND_RETURN(Array) ;
  CHECK_TYPE_AND_RETURN(BigInt) ;
  CHECK_TYPE_AND_RETURN(Boolean) ;
  CHECK_TYPE_AND_RETURN(Int32) ;
  CHECK_TYPE_AND_RETURN(Uint32) ;
  CHECK_TYPE_AND_RETURN(Date) ;
  CHECK_TYPE_AND_RETURN(ArgumentsObject) ;
  CHECK_TYPE_AND_RETURN(BigIntObject) ;
  CHECK_TYPE_AND_RETURN(BooleanObject) ;
  CHECK_TYPE_AND_RETURN(NumberObject) ;
  CHECK_TYPE_AND_RETURN(StringObject) ;
  CHECK_TYPE_AND_RETURN(SymbolObject) ;
  CHECK_TYPE_AND_RETURN(NativeError) ;
  CHECK_TYPE_AND_RETURN(RegExp) ;
  CHECK_TYPE_AND_RETURN(AsyncFunction) ;
  CHECK_TYPE_AND_RETURN(GeneratorFunction) ;
  CHECK_TYPE_AND_RETURN(GeneratorObject) ;
  CHECK_TYPE_AND_RETURN(Promise) ;
  CHECK_TYPE_AND_RETURN(Map) ;
  CHECK_TYPE_AND_RETURN(Set) ;
  CHECK_TYPE_AND_RETURN(MapIterator) ;
  CHECK_TYPE_AND_RETURN(SetIterator) ;
  CHECK_TYPE_AND_RETURN(WeakMap) ;
  CHECK_TYPE_AND_RETURN(WeakSet) ;
  CHECK_TYPE_AND_RETURN(ArrayBuffer) ;
  CHECK_TYPE_AND_RETURN(ArrayBufferView) ;
  CHECK_TYPE_AND_RETURN(Uint8Array) ;
  CHECK_TYPE_AND_RETURN(Uint8ClampedArray) ;
  CHECK_TYPE_AND_RETURN(Int8Array) ;
  CHECK_TYPE_AND_RETURN(Uint16Array) ;
  CHECK_TYPE_AND_RETURN(Int16Array) ;
  CHECK_TYPE_AND_RETURN(Uint32Array) ;
  CHECK_TYPE_AND_RETURN(Int32Array) ;
  CHECK_TYPE_AND_RETURN(Float32Array) ;
  CHECK_TYPE_AND_RETURN(Float64Array) ;
  CHECK_TYPE_AND_RETURN(BigInt64Array) ;
  CHECK_TYPE_AND_RETURN(BigUint64Array) ;
  CHECK_TYPE_AND_RETURN(DataView) ;
  CHECK_TYPE_AND_RETURN(SharedArrayBuffer) ;
  CHECK_TYPE_AND_RETURN(Proxy) ;
  CHECK_TYPE_AND_RETURN(WebAssemblyCompiledModule) ;
  CHECK_TYPE_AND_RETURN(ModuleNamespaceObject) ;

  // We check some types at the end
  // because there are many classe to inherit them
  CHECK_TYPE_AND_RETURN(Number) ;
  CHECK_TYPE_AND_RETURN(Object) ;

  DCHECK(false) ;
  return ValueType::Unknown ;
}

uint64_t operator &(ValueType type1, ValueType type2) {
  return (static_cast<uint64_t>(type1) & static_cast<uint64_t>(type2)) ;
}

ValueType GetValueType(Local<Value> value) {
  return GetValueType(value.operator ->()) ;
}

// bool IsValueType(Value* value, ValueType type) {
//   return (GetValueType(value) & type) ;
// }
//
// bool IsValueType(Local<Value> value, ValueType type) {
//   return IsValueType(value.operator ->(), type) ;
// }

const char* ValueTypeToUtf8(ValueType type) {
  # define TYPE_TO_STRING(ckecked_type) \
      if (type == ValueType::ckecked_type) { return #ckecked_type ; }

  TYPE_TO_STRING(Unknown) ;
  TYPE_TO_STRING(Null) ;
  TYPE_TO_STRING(String) ;
  TYPE_TO_STRING(Symbol) ;
  TYPE_TO_STRING(Function) ;
  TYPE_TO_STRING(Array) ;
  TYPE_TO_STRING(Object) ;
  TYPE_TO_STRING(BigInt) ;
  TYPE_TO_STRING(Boolean) ;
  TYPE_TO_STRING(Number) ;
  TYPE_TO_STRING(Int32) ;
  TYPE_TO_STRING(Uint32) ;
  TYPE_TO_STRING(Date) ;
  TYPE_TO_STRING(ArgumentsObject) ;
  TYPE_TO_STRING(BigIntObject) ;
  TYPE_TO_STRING(BooleanObject) ;
  TYPE_TO_STRING(NumberObject) ;
  TYPE_TO_STRING(StringObject) ;
  TYPE_TO_STRING(SymbolObject) ;
  TYPE_TO_STRING(NativeError) ;
  TYPE_TO_STRING(RegExp) ;
  TYPE_TO_STRING(AsyncFunction) ;
  TYPE_TO_STRING(GeneratorFunction) ;
  TYPE_TO_STRING(GeneratorObject) ;
  TYPE_TO_STRING(Promise) ;
  TYPE_TO_STRING(Map) ;
  TYPE_TO_STRING(Set) ;
  TYPE_TO_STRING(MapIterator) ;
  TYPE_TO_STRING(SetIterator) ;
  TYPE_TO_STRING(WeakMap) ;
  TYPE_TO_STRING(WeakSet) ;
  TYPE_TO_STRING(ArrayBuffer) ;
  TYPE_TO_STRING(ArrayBufferView) ;
  TYPE_TO_STRING(Uint8Array) ;
  TYPE_TO_STRING(Uint8ClampedArray) ;
  TYPE_TO_STRING(Int8Array) ;
  TYPE_TO_STRING(Uint16Array) ;
  TYPE_TO_STRING(Int16Array) ;
  TYPE_TO_STRING(Uint32Array) ;
  TYPE_TO_STRING(Int32Array) ;
  TYPE_TO_STRING(Float32Array) ;
  TYPE_TO_STRING(Float64Array) ;
  TYPE_TO_STRING(BigInt64Array) ;
  TYPE_TO_STRING(BigUint64Array) ;
  TYPE_TO_STRING(DataView) ;
  TYPE_TO_STRING(SharedArrayBuffer) ;
  TYPE_TO_STRING(Proxy) ;
  TYPE_TO_STRING(WebAssemblyCompiledModule) ;
  TYPE_TO_STRING(ModuleNamespaceObject) ;

  DCHECK(false) ;
  return "Unknown" ;
}

// Class-wrapper of Json gaps if we use a formatted ouput
class JsonGap {
 public:
  JsonGap(JsonGapArray& gaps, FormattedJson formatted, int index)
    : gaps_(gaps), formatted_(formatted), index_(index) {
    if (gaps.size() == 0) {
      gaps.push_back("") ;
    }

    for (int i = (int)gaps.size(); i <= index; ++i) {
      gaps.push_back(kJsonGap[*this] + gaps[i - 1]) ;
    }
  }

  JsonGap(const JsonGap& parent)
    : JsonGap(parent.gaps_, parent.formatted_, parent.index_ + 1) {}

  int index() const { return index_ ; }

  operator const char*() const {
    return gaps_[index_].c_str() ;
  }

  operator int() const {
    return formatted_ == FormattedJson::True ? 1 : 0 ;
  }

 private:
  JsonGap() = delete ;

  JsonGapArray& gaps_ ;
  FormattedJson formatted_ ;
  int index_ ;
};

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
  void SerializeFunction(Local<Function> value, uint64_t id, JsonGap& gap) ;
  void SerializeKeyValueArray(Local<Array> value, JsonGap& gap) ;
  void SerializeMap(Local<Map> value, uint64_t id, JsonGap& gap) ;
  void SerializeNumber(Local<Number> value, uint64_t id, JsonGap& gap) ;
  void SerializeObject(Local<Object> value, uint64_t id, JsonGap& gap) ;
  void SerializePrimitiveObject(
      Local<Object> value, uint64_t id, JsonGap& gap) ;
  void SerializeProcessedValue(Local<Value> value, uint64_t id, JsonGap& gap) ;
  void SerializeString(Local<String> value, uint64_t id, JsonGap& gap) ;
  void SerializeSymbol(Local<Symbol> value, uint64_t id, JsonGap& gap) ;
  void SerializeValue(Local<Value> value, JsonGap& gap) ;
  void SerializeWeakMap(Local<Object> value, uint64_t id, JsonGap& gap) ;

  std::string ValueToField(Local<Value> value, JsonGap& gap) ;

  Local<Context> context_ ;
  JsonGapArray gaps_ ;
  JsonGap default_gap_ ;
  std::map<i::Object*, uint64_t> processed_values_ ;
  std::ostream* result_ = nullptr ;

  static const uint64_t kEmptyId ;

  DISALLOW_COPY_AND_ASSIGN(ValueSerializer) ;
};

// Writes JsonGap into std::ostream
std::ostream& operator <<(std::ostream& os, const JsonGap& gap) {
  os << (const char*)gap ;
  return os ;
}

// Checks symbol for necessaty to be escape
bool DoNotEscape(char c) {
  return c >= '#' && c <= '~' && c != '\\' ;
}

// Encodes a string for Json
std::string EncodeJsonString(const std::string& str) {
  std::string result ;
  int begin = 0 ;
  for (int index = 0, length = (int)str.length(); index < length; ++index) {
    if (!DoNotEscape(str.at(index))) {
      result.append(str.c_str() + begin, index - begin) ;
      result += &JsonEscapeTable[str.at(index) * kJsonEscapeTableEntrySize] ;
      begin = index + 1 ;
    }
  }

  result += str.c_str() + begin ;
  return result ;
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
  JsonGap array_gap(child_gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  *result_ << child_gap << kJsonFieldId[gap] << id ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldType[gap]
      << JSON_STRING(ValueTypeToUtf8(ValueType::Array)) ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldLength[gap]
      << value->Length() ;
  // Serialize a array content
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldValue[gap] ;
  if (value->Length()) {
    bool comma = false ;
    *result_ << kJsonLeftSquareBracket[gap] ;
    for (uint32_t i = 0, size = value->Length(); i < size; ++i) {
      if (comma) {
        *result_ << kJsonComma[gap] ;
      } else {
        comma = true ;
      }

      *result_ << array_gap ;
      TryCatch try_catch(context_->GetIsolate()) ;
      v8::Local<v8::Value> array_value ;
      if (!value->Get(context_, i).ToLocal(&array_value) &&
          try_catch.HasCaught()) {
        *result_ << JSON_STRING(ValueToUtf8(context_, try_catch.Exception())) ;
        continue ;
      }

      SerializeValue(array_value, array_gap) ;
    }

    *result_ << kJsonNewLine[gap] << child_gap << kJsonRightSquareBracket[gap] ;
  } else {
    *result_ << kJsonEmptyArray[gap] ;
  }

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
    if (!value->Get(context_, i).ToLocal(&key_value) && try_catch.HasCaught()) {
      *result_ << JSON_STRING(ValueToUtf8(context_, try_catch.Exception())) ;
    } else {
      SerializeValue(key_value, key_value_gap) ;
    }

    *result_ << kJsonComma[gap] ;

    // Serialize value
    *result_ << key_value_gap ;
    v8::Local<v8::Value> value_value ;
    if (!value->Get(context_, i + 1).ToLocal(&value_value) &&
        try_catch.HasCaught()) {
      *result_ << JSON_STRING(ValueToUtf8(context_, try_catch.Exception())) ;
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
  JsonGap item_gap(child_gap) ;
  JsonGap key_value_gap(item_gap) ;
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
      if (!value->Get(context_, key).ToLocal(&child_value) &&
          try_catch.HasCaught()) {
        *result_ << JSON_STRING(ValueToUtf8(context_, try_catch.Exception())) ;
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
      *result_ << number_value ;
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

  if (value_type == ValueType::Unknown) {
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
  } else if (value_type == ValueType::String) {
    SerializeString(Local<String>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::Symbol) {
    SerializeSymbol(Local<Symbol>::Cast(value), id, gap) ;
    return ;
  } else if (value_type == ValueType::WeakMap) {
    SerializeWeakMap(Local<Object>::Cast(value), id, gap) ;
    return ;
  }

#ifdef DEBUG
  *result_ << JSON_STRING(
      "Don't have a serializer for \'" +
      std::string(ValueTypeToUtf8(value_type)) + "\'") ;
#else
  result << kJsonValueUndefined ;
#endif  // DEBUG
}

void ValueSerializer::SerializeWeakMap(
    Local<Object> value, uint64_t id, JsonGap& gap) {
  JsonGap child_gap(gap) ;
  JsonGap item_gap(child_gap) ;
  JsonGap key_value_gap(item_gap) ;
  *result_ << kJsonLeftBracket[gap] ;
  *result_ << child_gap << kJsonFieldId[gap] << id ;
  *result_ << kJsonComma[gap] << child_gap << kJsonFieldType[gap]
      << JSON_STRING(ValueTypeToUtf8(ValueType::WeakMap)) ;
  *result_ << kJsonComma[gap] ;

  TryCatch try_catch(context_->GetIsolate()) ;

  // Try to get content
  Local<Array> map ;
  bool is_key_value = false ;
  if (!value->PreviewEntries(&is_key_value).ToLocal(&map) &&
      try_catch.HasCaught()) {
    *result_ << child_gap << kJsonFieldValue[gap] ;
    *result_ << JSON_STRING(ValueToUtf8(context_, try_catch.Exception())) ;
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
