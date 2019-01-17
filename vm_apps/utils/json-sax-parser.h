// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_UTILS_JSON_SAX_PARSER_H_
#define V8_VM_APPS_UTILS_JSON_SAX_PARSER_H_

#include <functional>

#include "src/base/macros.h"
#include "vm_apps/utils/app-utils.h"

class JsonSaxParser {
 public:
  enum Options {
    // Parses the input strictly according to RFC 4627, except for where noted
    // above.
    JSON_PARSE_RFC = 0,

    // Allows commas to exist after the last element in structures.
    JSON_ALLOW_TRAILING_COMMAS = 1 << 0,

    // If set the parser replaces invalid characters with the Unicode
    // replacement character (U+FFFD). If not set, invalid characters trigger
    // a hard error and parsing fails.
    JSON_REPLACE_INVALID_CHARACTERS = 1 << 1,
  };

  struct Callbacks {
    std::function<Error()> null_callback = nullptr ;
    std::function<Error(bool)> boolean_callback = nullptr ;
    std::function<Error(std::int64_t)> integer_callback = nullptr ;
    std::function<Error(double)> double_callback = nullptr ;
    std::function<Error(const char*, std::size_t)>
        string_callback = nullptr ;
    std::function<Error()> start_map_callback = nullptr ;
    std::function<Error(const char*, std::size_t)>
        map_key_callback = nullptr ;
    std::function<Error()> end_map_callback = nullptr ;
    std::function<Error(const char*)> start_array_callback = nullptr ;
    std::function<Error(const char*)> end_array_callback = nullptr ;
  };

  explicit JsonSaxParser(const Callbacks& callbacks, int options) ;
  ~JsonSaxParser() ;

  // Parses the input string according to the set options.
  Error Parse(const char* input, std::int32_t size) ;

  // Returns the error code.
  Error error() const ;

  // Returns the human-friendly error message.
  std::string GetErrorMessage() const ;

  // Returns the error line number if parse error happened. Otherwise always
  // returns 0.
  std::int32_t error_line() const ;

  // Returns the error column number if parse error happened. Otherwise always
  // returns 0.
  std::int32_t error_column() const ;

 private:
  enum Token {
    T_OBJECT_BEGIN,           // {
    T_OBJECT_END,             // }
    T_ARRAY_BEGIN,            // [
    T_ARRAY_END,              // ]
    T_STRING,
    T_NUMBER,
    T_BOOL_TRUE,              // true
    T_BOOL_FALSE,             // false
    T_NULL,                   // null
    T_LIST_SEPARATOR,         // ,
    T_OBJECT_PAIR_SEPARATOR,  // :
    T_END_OF_INPUT,
    T_INVALID_TOKEN,
  };

  // A helper class used for parsing strings. One optimization performed is to
  // avoid unnecessary std::string copies. This is not possible if the input
  // string needs to be decoded from UTF-16 to UTF-8, or if an escape sequence
  // causes characters to be skipped. This class centralizes that logic.
  class StringBuilder {
   public:
    // Empty constructor. Used for creating a builder with which to assign to.
    StringBuilder() ;

    // |pos| is the beginning of an input string, excluding the |"|.
    explicit StringBuilder(const char* pos) ;

    ~StringBuilder() ;

    StringBuilder& operator =(StringBuilder&& other) ;

    // Either increases the |length_| of the string or copies the character if
    // the StringBuilder has been converted. |c| must be in the basic ASCII
    // plane; all other characters need to be in UTF-8 units, appended with
    // AppendString below.
    void Append(const char& c) ;

    // Appends a string to the std::string. Must be Convert()ed to use.
    void AppendString(const char* str, size_t len) ;

    // Converts the builder from its default const char* to a full std::string,
    // performing a copy. Once a builder is converted, it cannot be made a
    // const char* again.
    void Convert() ;

    // Returns the builder as a std::string.
    const std::string& AsString() ;

    // Returns the builder as a string, invalidating all state. This allows
    // the internal string buffer representation to be destructively moved
    // in cases where the builder will not be needed any more.
    std::string DestructiveAsString() ;

   private:
    // The beginning of the input string.
    const char* pos_;

    // Number of bytes in |pos_| that make up the string being built.
    size_t length_;

    // The copied string representation. Will be unset until Convert() is
    // called.
    std::shared_ptr<std::string> string_;
  };

  // Quick check that the stream has capacity to consume |length| more bytes.
  bool CanConsume(int length) ;

  // The basic way to consume a single character in the stream. Consumes one
  // byte of the input stream and returns a pointer to the rest of it.
  const char* NextChar() ;

  // Performs the equivalent of NextChar N times.
  void NextNChars(int n) ;

  // Skips over whitespace and comments to find the next token in the stream.
  // This does not advance the parser for non-whitespace or comment chars.
  Token GetNextToken() ;

  // Consumes whitespace characters and comments until the next non-that is
  // encountered.
  void EatWhitespaceAndComments() ;
  // Helper function that consumes a comment, assuming that the parser is
  // currently wound to a '/'.
  bool EatComment() ;

  // Calls GetNextToken() and then ParseToken().
  Error ParseNextToken() ;

  // Takes a token that represents the start of a Value ("a structural token"
  // in RFC terms) and consumes it, returning the result as a Value.
  Error ParseToken(Token token) ;

  // Assuming that the parser is currently wound to '{', this parses a JSON
  // object into a DictionaryValue.
  Error ConsumeDictionary() ;

  // Assuming that the parser is wound to '[', this parses a JSON list into a
  // std::unique_ptr<ListValue>.
  Error ConsumeList() ;

  // Calls through ConsumeStringRaw and wraps it in a value.
  Error ConsumeString() ;

  // Assuming that the parser is wound to a double quote, this parses a string,
  // decoding any escape sequences and converts UTF-16 to UTF-8. Returns true on
  // success and places result into |out|. Returns false on failure with
  // error information set.
  Error ConsumeStringRaw(StringBuilder* out) ;
  // Helper function for ConsumeStringRaw() that consumes the next four or 10
  // bytes (parser is wound to the first character of a HEX sequence, with the
  // potential for consuming another \uXXXX for a surrogate). Returns true on
  // success and places the UTF8 code units in |dest_string|, and false on
  // failure.
  bool DecodeUTF16(std::string* dest_string) ;
  // Helper function for ConsumeStringRaw() that takes a single code point,
  // decodes it into UTF-8 units, and appends it to the given builder. The
  // point must be valid.
  void DecodeUTF8(const int32_t& point, StringBuilder* dest) ;

  // Assuming that the parser is wound to the start of a valid JSON number,
  // this parses and converts it to either an int or double value.
  Error ConsumeNumber() ;
  // Helper that reads characters that are ints. Returns true if a number was
  // read and false on error.
  bool ReadInt(bool allow_leading_zeros) ;

  // Consumes the literal values of |true|, |false|, and |null|, assuming the
  // parser is wound to the first character of any of those.
  Error ConsumeLiteral() ;

  // Compares two string buffers of a given length.
  static bool StringsAreEqual(const char* left, const char* right, size_t len) ;

  // Sets the error information to |code| at the current column, based on
  // |index_| and |index_last_line_|, with an optional positive/negative
  // adjustment by |column_adjust|.
  void ReportError(Error code, int column_adjust) ;

  // Given the line and column number of an error, formats one of the error
  // message contants from json_reader.h for human display.
  static std::string FormatErrorMessage(
      int line, int column, const std::string& description) ;

  // Callbacks
  Callbacks callbacks_ ;

  // Options that control parsing.
  const int options_ ;

  // Pointer to the start of the input data.
  const char* start_pos_ ;

  // Pointer to the current position in the input data. Equivalent to
  // |start_pos_ + index_|.
  const char* pos_ ;

  // Pointer to the last character of the input data.
  const char* end_pos_ ;

  // The index in the input stream to which the parser is wound.
  int index_ ;

  // The number of times the parser has recursed (current stack depth).
  int stack_depth_ ;

  // The line number that the parser is at currently.
  int line_number_ ;

  // The last value of |index_| on the previous line.
  int index_last_line_ ;

  // Error information.
  Error error_ ;
  int error_line_ ;
  int error_column_ ;

  DISALLOW_COPY_AND_ASSIGN(JsonSaxParser) ;
};

#endif  // V8_VM_APPS_UTILS_JSON_SAX_PARSER_H_
