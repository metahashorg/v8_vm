// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/utils/json-sax-parser.h"

#include <cctype>
#include <cmath>

#include "src/base/v8-fallthrough.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#include "third_party/icu/source/common/unicode/utf8.h"
#include "vm_apps/utils/string-number-conversions.h"

#define ReportErrorM(error, ...) \
    ReportError(error, __VA_ARGS__) ; \
    V8_ERROR_ADD_MSG_BACK_OFFSET( \
        error, \
        FormatErrorMessage( \
            origin_.c_str(), error_line_, error_column_, "json is invalid"), \
        1) ;
#define ReportCallbackErrorM(error, ...) \
    ReportError(error, __VA_ARGS__) ; \
    V8_ERROR_ADD_MSG( \
        error, \
        FormatErrorMessage( \
            origin_.c_str(), error_line_, error_column_, "json is invalid")) ;

namespace {

const int32_t kExtendedASCIIStart = 0x80 ;

// This is U+FFFD.
const char kUnicodeReplacementString[] = "\xEF\xBF\xBD" ;

// Chosen to support 99.9% of documents found in the wild late 2016.
const int kStackMaxDepth = 200 ;

// Simple class that checks for maximum recursion/"stack overflow."
class StackMarker {
 public:
  explicit StackMarker(int* depth) : depth_(depth) {
    ++(*depth_) ;
    // TODO: DCHECK_LE(*depth_, kStackMaxDepth);
  }
  ~StackMarker() {
    --(*depth_) ;
  }

  bool IsTooDeep() const {
    return *depth_ >= kStackMaxDepth ;
  }

 private:
  int* const depth_ ;

  DISALLOW_COPY_AND_ASSIGN(StackMarker) ;
};

inline bool IsValidCharacter(std::uint32_t code_point) {
  // Excludes non-characters (U+FDD0..U+FDEF, and all codepoints ending in
  // 0xFFFE or 0xFFFF) from the set of valid code points.
  return code_point < 0xD800u || (code_point >= 0xE000u &&
      code_point < 0xFDD0u) || (code_point > 0xFDEFu &&
      code_point <= 0x10FFFFu && (code_point & 0xFFFEu) != 0xFFFEu);
}

}  // namespace

JsonSaxParser::JsonSaxParser(const Callbacks& callbacks, int options)
  : callbacks_(callbacks),
    options_(options),
    start_pos_(nullptr),
    pos_(nullptr),
    end_pos_(nullptr),
    index_(0),
    stack_depth_(0),
    line_number_(0),
    index_last_line_(0),
    error_(errOk),
    error_line_(0),
    error_column_(0) {}

JsonSaxParser::~JsonSaxParser() {}

Error JsonSaxParser::Parse(
    const char* input, const char* origin, std::int32_t size) {
  origin_ = origin ;
  start_pos_ = input ;
  pos_ = start_pos_ ;
  end_pos_ = start_pos_ + size ;
  index_ = 0 ;
  line_number_ = 1 ;
  index_last_line_ = 0 ;

  error_ = errOk ;
  error_line_ = 0 ;
  error_column_ = 0 ;

  // When the input JSON string starts with a UTF-8 Byte-Order-Mark
  // <0xEF 0xBB 0xBF>, advance the start position to avoid the
  // ParseNextToken function mis-treating a Unicode BOM as an invalid
  // character and returning NULL.
  if (CanConsume(3) && static_cast<uint8_t>(*pos_) == 0xEF &&
      static_cast<uint8_t>(*(pos_ + 1)) == 0xBB &&
      static_cast<uint8_t>(*(pos_ + 2)) == 0xBF) {
    NextNChars(3) ;
  }

  // Parse the first and any nested tokens.
  Error result = ParseNextToken() ;
  if (V8_ERROR_FAILED(result)) {
    printf("ERROR: Json is invalid\n") ;
    return result ;
  }

  // Make sure the input stream is at an end.
  if (GetNextToken() != T_END_OF_INPUT) {
    if (!CanConsume(1) || (NextChar() && GetNextToken() != T_END_OF_INPUT)) {
      result = errJsonUnexpectedDataAfterRoot ;
      ReportErrorM(result, 1) ;
    }
  }

  return result ;
}

Error JsonSaxParser::error() const {
  return error_ ;
}

std::string JsonSaxParser::GetErrorMessage() const {
  return FormatErrorMessage(
      origin_.c_str(), error_line_, error_column_, error_.description()) ;
}

int JsonSaxParser::error_line() const {
  return error_line_ ;
}

int JsonSaxParser::error_column() const {
  return error_column_ ;
}

JsonSaxParser::StringBuilder::StringBuilder() : StringBuilder(nullptr) {}

JsonSaxParser::StringBuilder::StringBuilder(const char* pos)
    : pos_(pos), length_(0) {}

JsonSaxParser::StringBuilder::~StringBuilder() {}

JsonSaxParser::StringBuilder& JsonSaxParser::StringBuilder::operator =(
    StringBuilder&& other) = default ;

void JsonSaxParser::StringBuilder::Append(const char& c) {
  // TODO:
  // DCHECK_GE(c, 0) ;
  // DCHECK_LT(static_cast<unsigned char>(c), 128) ;

  if (string_) {
    string_->push_back(c) ;
  } else {
    ++length_ ;
  }
}

void JsonSaxParser::StringBuilder::AppendString(const char* str, size_t len) {
  // TODO: DCHECK(string_) ;
  string_->append(str, len) ;
}

void JsonSaxParser::StringBuilder::Convert() {
  if (string_) {
    return ;
  }

  string_ = std::make_shared<std::string>(pos_, length_) ;
}

const std::string& JsonSaxParser::StringBuilder::AsString() {
  if (!string_) {
    Convert() ;
  }

  return *string_ ;
}

std::string JsonSaxParser::StringBuilder::DestructiveAsString() {
  if (string_) {
    return std::move(*string_) ;
  }

  return std::string(pos_, length_) ;
}

inline bool JsonSaxParser::CanConsume(int length) {
  return pos_ + length <= end_pos_ ;
}

const char* JsonSaxParser::NextChar() {
  // TODO: DCHECK(CanConsume(1)) ;
  ++index_ ;
  ++pos_ ;
  return pos_ ;
}

void JsonSaxParser::NextNChars(int n) {
  // TODO: DCHECK(CanConsume(n)) ;
  index_ += n ;
  pos_ += n ;
}

JsonSaxParser::Token JsonSaxParser::GetNextToken() {
  EatWhitespaceAndComments() ;
  if (!CanConsume(1))
    return T_END_OF_INPUT ;

  switch (*pos_) {
    case '{':
      return T_OBJECT_BEGIN ;
    case '}':
      return T_OBJECT_END ;
    case '[':
      return T_ARRAY_BEGIN ;
    case ']':
      return T_ARRAY_END ;
    case '"':
      return T_STRING ;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '-':
      return T_NUMBER ;
    case 't':
      return T_BOOL_TRUE ;
    case 'f':
      return T_BOOL_FALSE ;
    case 'n':
      return T_NULL ;
    case ',':
      return T_LIST_SEPARATOR ;
    case ':':
      return T_OBJECT_PAIR_SEPARATOR ;
    default:
      return T_INVALID_TOKEN ;
  }
}

void JsonSaxParser::EatWhitespaceAndComments() {
  while (pos_ < end_pos_) {
    switch (*pos_) {
      case '\r':
      case '\n':
        index_last_line_ = index_ ;
        // Don't increment line_number_ twice for "\r\n".
        if (!(*pos_ == '\n' && pos_ > start_pos_ && *(pos_ - 1) == '\r')) {
          ++line_number_ ;
        }
        V8_FALLTHROUGH ;
      case ' ':
      case '\t':
        NextChar() ;
        break ;
      case '/':
        if (!EatComment()) {
          return ;
        }
        break ;
      default:
        return ;
    }
  }
}

bool JsonSaxParser::EatComment() {
  if (*pos_ != '/' || !CanConsume(1)) {
    return false ;
  }

  NextChar() ;

  if (!CanConsume(1)) {
    return false ;
  }

  if (*pos_ == '/') {
    // Single line comment, read to newline.
    while (CanConsume(1)) {
      if (*pos_ == '\n' || *pos_ == '\r') {
        return true ;
      }

      NextChar()  ;
    }
  } else if (*pos_ == '*') {
    char previous_char = '\0' ;
    // Block comment, read until end marker.
    while (CanConsume(1)) {
      if (previous_char == '*' && *pos_ == '/') {
        // EatWhitespaceAndComments will inspect pos_, which will still be on
        // the last / of the comment, so advance once more (which may also be
        // end of input).
        NextChar() ;
        return true ;
      }

      previous_char = *pos_ ;
      NextChar() ;
    }

    // If the comment is unterminated, GetNextToken will report T_END_OF_INPUT.
  }

  return false ;
}

Error JsonSaxParser::ParseNextToken() {
  return ParseToken(GetNextToken()) ;
}

Error JsonSaxParser::ParseToken(Token token) {
  switch (token) {
    case T_OBJECT_BEGIN:
      return ConsumeDictionary() ;
    case T_ARRAY_BEGIN:
      return ConsumeList() ;
    case T_STRING:
      return ConsumeString() ;
    case T_NUMBER:
      return ConsumeNumber() ;
    case T_BOOL_TRUE:
    case T_BOOL_FALSE:
    case T_NULL:
      return ConsumeLiteral() ;
    default:
      Error result = errJsonUnexpectedToken ;
      ReportErrorM(result, 1) ;
      return result ;
  }
}

Error JsonSaxParser::ConsumeDictionary() {
  Error result = errOk ;
  if (*pos_ != '{') {
    result = errJsonUnexpectedToken ;
    ReportErrorM(result, 1) ;
    return result ;
  }

  StackMarker depth_check(&stack_depth_);
  if (depth_check.IsTooDeep()) {
    result = errJsonTooMuchNesting ;
    ReportErrorM(result, 1) ;
    return result ;
  }

  if (callbacks_.start_map_callback) {
    Error result = callbacks_.start_map_callback() ;
    if (V8_ERROR_FAILED(result)) {
      ReportCallbackErrorM(result, 1) ;
      return result ;
    }
  }

  NextChar() ;
  Token token = GetNextToken() ;
  while (token != T_OBJECT_END) {
    if (token != T_STRING) {
      result = errJsonUnquotedDictionaryKey ;
      ReportErrorM(result, 1) ;
      return result ;
    }

    // First consume the key.
    StringBuilder key ;
    Error result = ConsumeStringRaw(&key) ;
    if (V8_ERROR_FAILED(result)) {
      return result ;
    }

    if (callbacks_.map_key_callback) {
      std::string key_str = key.DestructiveAsString() ;
      result = callbacks_.map_key_callback(key_str.c_str(), key_str.size()) ;
      if (V8_ERROR_FAILED(result)) {
        ReportCallbackErrorM(result, (int)-key_str.size()) ;
        return result ;
      }
    }

    // Read the separator.
    NextChar() ;
    token = GetNextToken() ;
    if (token != T_OBJECT_PAIR_SEPARATOR) {
      result = errJsonSyntaxError ;
      ReportErrorM(result, 1) ;
      return result ;
    }

    // The next token is the value.
    NextChar() ;
    result = ParseNextToken() ;
    if (V8_ERROR_FAILED(result)) {
      // ReportErrorM from deeper level.
      return result ;
    }

    NextChar() ;
    token = GetNextToken() ;
    if (token == T_LIST_SEPARATOR) {
      NextChar() ;
      token = GetNextToken() ;
      if (token == T_OBJECT_END && !(options_ & JSON_ALLOW_TRAILING_COMMAS)) {
        result = errJsonTrailingComma ;
        ReportErrorM(result, 1) ;
        return result ;
      }
    } else if (token != T_OBJECT_END) {
      result = errJsonSyntaxError ;
      ReportErrorM(result, 0) ;
      return result ;
    }
  }

  if (callbacks_.end_map_callback) {
    Error result = callbacks_.end_map_callback() ;
    if (V8_ERROR_FAILED(result)) {
      ReportCallbackErrorM(result, 1) ;
      return result ;
    }
  }

  return errOk ;
}

Error JsonSaxParser::ConsumeList() {
  Error result = errOk ;
  if (*pos_ != '[') {
    result = errJsonUnexpectedToken ;
    ReportErrorM(result, 1) ;
    return result ;
  }

  StackMarker depth_check(&stack_depth_);
  if (depth_check.IsTooDeep()) {
    result = errJsonTooMuchNesting ;
    ReportErrorM(result, 1) ;
    return result ;
  }

  if (callbacks_.start_array_callback) {
    result = callbacks_.start_array_callback(pos_) ;
    if (V8_ERROR_FAILED(result)) {
      ReportCallbackErrorM(result, 1) ;
      return result ;
    }
  }

  NextChar() ;
  Token token = GetNextToken() ;
  while (token != T_ARRAY_END) {
    Error result = ParseToken(token) ;
    if (V8_ERROR_FAILED(result)) {
      // ReportErrorM from deeper level.
      return result ;
    }

    NextChar() ;
    token = GetNextToken() ;
    if (token == T_LIST_SEPARATOR) {
      NextChar() ;
      token = GetNextToken() ;
      if (token == T_ARRAY_END && !(options_ & JSON_ALLOW_TRAILING_COMMAS)) {
        result = errJsonTrailingComma ;
        ReportErrorM(result, 1) ;
        return result ;
      }
    } else if (token != T_ARRAY_END) {
      result = errJsonSyntaxError ;
      ReportErrorM(result, 1) ;
      return result ;
    }
  }

  if (callbacks_.end_array_callback) {
    result = callbacks_.end_array_callback(pos_) ;
    if (V8_ERROR_FAILED(result)) {
      ReportCallbackErrorM(result, 1) ;
      return result ;
    }
  }

  return errOk ;
}

Error JsonSaxParser::ConsumeString() {
  StringBuilder string ;
  Error result = ConsumeStringRaw(&string) ;
  if (V8_ERROR_FAILED(result)) {
    return result ;
  }

  if (callbacks_.string_callback) {
    std::string value = string.DestructiveAsString() ;
    result = callbacks_.string_callback(value.c_str(), value.length()) ;
    if (V8_ERROR_FAILED(result)) {
      ReportCallbackErrorM(result, (int)-value.length()) ;
    }
  }

  return result ;
}

Error JsonSaxParser::ConsumeStringRaw(StringBuilder* out) {
  Error result = errOk ;
  if (*pos_ != '"') {
    result = errJsonUnexpectedToken ;
    ReportErrorM(result, 1) ;
    return result ;
  }

  // Strings are at minimum two characters: the surrounding double quotes.
  if (!CanConsume(2)) {
    result = errJsonSyntaxError ;
    ReportErrorM(result, 1) ;
    return result ;
  }

  // StringBuilder will internally build a StringPiece unless a UTF-16
  // conversion occurs, at which point it will perform a copy into a
  // std::string.
  StringBuilder string(NextChar()) ;

  // Handle the empty string case early.
  if (*pos_ == '"') {
    *out = std::move(string) ;
    return errOk ;
  }

  int length = static_cast<int>(end_pos_ - start_pos_) ;
  int32_t next_char = 0 ;

  // There must always be at least two characters left in the stream: the next
  // string character and the terminating closing quote.
  while (CanConsume(2)) {
    int start_index = index_ ;
    pos_ = start_pos_ + index_ ;  // U8_NEXT is postcrement.
    U8_NEXT(start_pos_, index_, length, next_char) ;
    if (next_char < 0 || !IsValidCharacter(next_char)) {
      if ((options_ & JSON_REPLACE_INVALID_CHARACTERS) == 0) {
        result = errJsonUnsupportedEncoding ;
        ReportErrorM(result, 1) ;
        return result ;
      }

      U8_NEXT(start_pos_, start_index, length, next_char) ;
      string.Convert() ;
      string.AppendString(kUnicodeReplacementString,
                          arraysize(kUnicodeReplacementString) - 1) ;
      continue ;
    }

    if (next_char == '"') {
      --index_ ;  // Rewind by one because of U8_NEXT.
      *out = std::move(string) ;
      return errOk ;
    }

    // If this character is not an escape sequence...
    if (next_char != '\\') {
      if (next_char < kExtendedASCIIStart) {
        string.Append(static_cast<char>(next_char)) ;
      } else {
        DecodeUTF8(next_char, &string) ;
      }
    } else {
      // And if it is an escape sequence, the input string will be adjusted
      // (either by combining the two characters of an encoded escape sequence,
      // or with a UTF conversion), so using StringPiece isn't possible -- force
      // a conversion.
      string.Convert() ;

      if (!CanConsume(1)) {
        result = errJsonInvalidEscape ;
        ReportErrorM(result, 0) ;
        return result ;
      }

      NextChar() ;
      if (!CanConsume(1)) {
        result = errJsonInvalidEscape ;
        ReportErrorM(result, 0) ;
        return result ;
      }

      switch (*pos_) {
        // Allowed esape sequences:
        case 'x': {  // UTF-8 sequence.
          // UTF-8 \x escape sequences are not allowed in the spec, but they
          // are supported here for backwards-compatiblity with the old parser.
          if (!CanConsume(3)) {
            result = errJsonInvalidEscape ;
            ReportErrorM(result, 1) ;
            return result ;
          }

          std::int32_t hex_digit = 0 ;
          if (!HexStringToInt32(std::string(NextChar(), 2), &hex_digit) ||
              !IsValidCharacter(hex_digit)) {
            result = errJsonInvalidEscape ;
            ReportErrorM(result, -1) ;
            return result ;
          }

          NextChar() ;

          if (hex_digit < kExtendedASCIIStart) {
            string.Append(static_cast<char>(hex_digit)) ;
          } else {
            DecodeUTF8(hex_digit, &string) ;
          }
          break ;
        }
        case 'u': {  // UTF-16 sequence.
          // UTF units are of the form \uXXXX.
          if (!CanConsume(5)) {  // 5 being 'u' and four HEX digits.
            result = errJsonInvalidEscape ;
            ReportErrorM(result, 0) ;
            return result ;
          }

          // Skip the 'u'.
          NextChar() ;

          std::string utf8_units ;
          if (!DecodeUTF16(&utf8_units)) {
            result = errJsonInvalidEscape ;
            ReportErrorM(result, -1) ;
            return result ;
          }

          string.AppendString(utf8_units.data(), utf8_units.length()) ;
          break ;
        }
        case '"':
          string.Append('"') ;
          break ;
        case '\\':
          string.Append('\\') ;
          break ;
        case '/':
          string.Append('/') ;
          break ;
        case 'b':
          string.Append('\b') ;
          break ;
        case 'f':
          string.Append('\f') ;
          break ;
        case 'n':
          string.Append('\n') ;
          break ;
        case 'r':
          string.Append('\r') ;
          break ;
        case 't':
          string.Append('\t') ;
          break ;
        case 'v':  // Not listed as valid escape sequence in the RFC.
          string.Append('\v') ;
          break ;
        // All other escape squences are illegal.
        default:
          result = errJsonInvalidEscape ;
          ReportErrorM(result, 0) ;
          return result ;
      }
    }
  }

  result = errJsonSyntaxError ;
  ReportErrorM(result, 0) ;
  return result ;
}

bool JsonSaxParser::DecodeUTF16(std::string* dest_string) {
  if (!CanConsume(4)) {
    return false ;
  }

  // This is a 32-bit field because the shift operations in the
  // conversion process below cause MSVC to error about "data loss."
  // This only stores UTF-16 code units, though.
  // Consume the UTF-16 code unit, which may be a high surrogate.
  int code_unit16_high = 0 ;
  if (!HexStringToInt32(std::string(pos_, 4), &code_unit16_high)) {
    return false ;
  }

  // Only add 3, not 4, because at the end of this iteration, the parser has
  // finished working with the last digit of the UTF sequence, meaning that
  // the next iteration will advance to the next byte.
  NextNChars(3) ;

  // Used to convert the UTF-16 code units to a code point and then to a UTF-8
  // code unit sequence.
  char code_unit8[8] = { 0 } ;
  size_t offset = 0 ;

  // If this is a high surrogate, consume the next code unit to get the
  // low surrogate.
  if (U16_IS_SURROGATE(code_unit16_high)) {
    // Make sure this is the high surrogate. If not, it's an encoding
    // error.
    if (!U16_IS_SURROGATE_LEAD(code_unit16_high)) {
      return false ;
    }

    // Make sure that the token has more characters to consume the
    // lower surrogate.
    if (!CanConsume(6)) {  // 6 being '\' 'u' and four HEX digits.
      return false ;
    }

    if (*NextChar() != '\\' || *NextChar() != 'u') {
      return false ;
    }

    NextChar() ;  // Read past 'u'.
    int code_unit16_low = 0 ;
    if (!HexStringToInt32(std::string(pos_, 4), &code_unit16_low)) {
      return false ;
    }

    NextNChars(3) ;

    if (!U16_IS_TRAIL(code_unit16_low)) {
      return false ;
    }

    uint32_t code_point =
        U16_GET_SUPPLEMENTARY(code_unit16_high, code_unit16_low) ;
    if (!IsValidCharacter(code_point)) {
      return false ;
    }

    offset = 0 ;
    U8_APPEND_UNSAFE(code_unit8, offset, code_point) ;
  } else {
    // Not a surrogate.
    // TODO: DCHECK(CBU16_IS_SINGLE(code_unit16_high)) ;
    if (!IsValidCharacter(code_unit16_high)) {
      if ((options_ & JSON_REPLACE_INVALID_CHARACTERS) == 0) {
        return false ;
      }

      dest_string->append(kUnicodeReplacementString) ;
      return true ;
    }

    U8_APPEND_UNSAFE(code_unit8, offset, code_unit16_high) ;
  }

  dest_string->append(code_unit8, offset) ;
  return true ;
}

void JsonSaxParser::DecodeUTF8(const int32_t& point, StringBuilder* dest) {
  // TODO: DCHECK(IsValidCharacter(point));

  // Anything outside of the basic ASCII plane will need to be decoded from
  // int32_t to a multi-byte sequence.
  if (point < kExtendedASCIIStart) {
    dest->Append(static_cast<char>(point)) ;
  } else {
    char utf8_units[4] = { 0 } ;
    int offset = 0 ;
    U8_APPEND_UNSAFE(utf8_units, offset, point) ;
    dest->Convert() ;
    // CBU8_APPEND_UNSAFE can overwrite up to 4 bytes, so utf8_units may not be
    // zero terminated at this point.  |offset| contains the correct length.
    dest->AppendString(utf8_units, offset) ;
  }
}

Error JsonSaxParser::ConsumeNumber() {
  const char* num_start = pos_ ;
  const int start_index = index_ ;
  int end_index = start_index ;
  Error result = errOk ;

  if (*pos_ == '-') {
    NextChar() ;
  }

  if (!ReadInt(false)) {
    result = errJsonSyntaxError ;
    ReportErrorM(result, 1) ;
    return result ;
  }

  end_index = index_ ;

  // The optional fraction part.
  if (CanConsume(1) && *pos_ == '.') {
    NextChar() ;
    if (!ReadInt(true)) {
      result = errJsonSyntaxError ;
      ReportErrorM(result, 1) ;
      return result ;
    }

    end_index = index_ ;
  }

  // Optional exponent part.
  if (CanConsume(1) && (*pos_ == 'e' || *pos_ == 'E')) {
    NextChar() ;
    if (!CanConsume(1)) {
      result = errJsonSyntaxError ;
      ReportErrorM(result, 1) ;
      return result ;
    }

    if (*pos_ == '-' || *pos_ == '+') {
      NextChar() ;
    }

    if (!ReadInt(true)) {
      result = errJsonSyntaxError ;
      ReportErrorM(result, 1);
      return result ;
    }

    end_index = index_ ;
  }

  // ReadInt is greedy because numbers have no easily detectable sentinel,
  // so save off where the parser should be on exit (see Consume invariant at
  // the top of the header), then make sure the next token is one which is
  // valid.
  const char* exit_pos = pos_ - 1 ;
  int exit_index = index_ - 1 ;

  switch (GetNextToken()) {
    case T_OBJECT_END:
    case T_ARRAY_END:
    case T_LIST_SEPARATOR:
    case T_END_OF_INPUT:
      break;
    default:
      result = errJsonSyntaxError ;
      ReportErrorM(result, 1) ;
      return result ;
  }

  pos_ = exit_pos ;
  index_ = exit_index ;

  std::string num_string(num_start, end_index - start_index) ;

  std::int64_t num_int ;
  if (StringToInt64(num_string, &num_int)) {
    if (callbacks_.integer_callback) {
      result = callbacks_.integer_callback(num_int) ;
      if (V8_ERROR_FAILED(result)) {
        ReportCallbackErrorM(result, start_index - end_index) ;
      }
    }

    return result ;
  }

  double num_double ;
  if (StringToDouble(num_string, &num_double) &&
      std::isfinite(num_double)) {
    if (callbacks_.double_callback) {
      result = callbacks_.double_callback(num_double) ;
      if (V8_ERROR_FAILED(result)) {
        ReportCallbackErrorM(result, start_index - end_index) ;
      }
    }

    return result ;
  }

  result = errJsonSyntaxError ;
  ReportErrorM(result, start_index - end_index) ;
  return result ;
}

bool JsonSaxParser::ReadInt(bool allow_leading_zeros) {
  std::size_t len = 0 ;
  char first = 0 ;

  while (CanConsume(1)) {
    if (!std::isdigit(*pos_)) {
      break ;
    }

    if (len == 0) {
      first = *pos_ ;
    }

    ++len ;
    NextChar() ;
  }

  if (len == 0) {
    return false ;
  }

  if (!allow_leading_zeros && len > 1 && first == '0') {
    return false ;
  }

  return true ;
}

Error JsonSaxParser::ConsumeLiteral() {
  Error result = errOk ;
  switch (*pos_) {
    case 't': {
      const char kTrueLiteral[] = "true" ;
      const int kTrueLen = static_cast<int>(strlen(kTrueLiteral)) ;
      if (!CanConsume(kTrueLen) ||
          !StringsAreEqual(pos_, kTrueLiteral, kTrueLen)) {
        result = errJsonSyntaxError ;
        ReportErrorM(result, 1) ;
        return result ;
      }

      if (callbacks_.boolean_callback) {
        result = callbacks_.boolean_callback(true) ;
        if (V8_ERROR_FAILED(result)) {
          ReportCallbackErrorM(result, 1) ;
        }
      }

      NextNChars(kTrueLen - 1) ;
      return result ;
    }
    case 'f': {
      const char kFalseLiteral[] = "false" ;
      const int kFalseLen = static_cast<int>(strlen(kFalseLiteral));
      if (!CanConsume(kFalseLen) ||
          !StringsAreEqual(pos_, kFalseLiteral, kFalseLen)) {
        result = errJsonSyntaxError ;
        ReportErrorM(result, 1) ;
        return result ;
      }

      Error result = errOk ;
      if (callbacks_.boolean_callback) {
        result = callbacks_.boolean_callback(false) ;
        if (V8_ERROR_FAILED(result)) {
          ReportCallbackErrorM(result, 1) ;
        }
      }

      NextNChars(kFalseLen - 1) ;
      return result ;
    }
    case 'n': {
      const char kNullLiteral[] = "null" ;
      const int kNullLen = static_cast<int>(strlen(kNullLiteral));
      if (!CanConsume(kNullLen) ||
          !StringsAreEqual(pos_, kNullLiteral, kNullLen)) {
        result = errJsonSyntaxError ;
        ReportErrorM(result, 1) ;
        return result ;
      }

      if (callbacks_.null_callback) {
        result = callbacks_.null_callback() ;
        if (V8_ERROR_FAILED(result)) {
          ReportCallbackErrorM(result, 1) ;
        }
      }

      NextNChars(kNullLen - 1);
      return result ;
    }
    default:
      result = errJsonUnexpectedToken ;
      ReportErrorM(result, 1) ;
      return result ;
  }
}

bool JsonSaxParser::StringsAreEqual(
    const char* left, const char* right, size_t len) {
  return strncmp(left, right, len) == 0 ;
}

void JsonSaxParser::ReportError(Error& error, int column_adjust) {
  error_ = error ;
  error_line_ = line_number_ ;
  error_column_ = index_ - index_last_line_ + column_adjust ;
}

// static
std::string JsonSaxParser::FormatErrorMessage(
    const char* origin, int line, int column, const std::string& description) {
  if (line || column) {
    return (origin ? StringPrintf("Origin:\'%s\' ", origin) : "") +
        StringPrintf("Line:%i Column:%i - %s",
            line, column, description.c_str()) ;
  }

  return description ;
}
