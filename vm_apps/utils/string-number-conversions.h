// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_UTILS_STRING_NUMBER_CONVERSIONS_H_
#define V8_VM_APPS_UTILS_STRING_NUMBER_CONVERSIONS_H_

#include <stddef.h>

#include <cstdint>
#include <string>
#include <vector>

// Number -> string conversions ------------------------------------------------

std::string Int32ToString(std::int32_t value) ;
std::wstring Int32ToString16(std::int32_t value) ;

std::string Uint32ToString(std::uint32_t value) ;
std::wstring Uint32ToString16(std::uint32_t value) ;

std::string Int64ToString(std::int64_t value) ;
std::wstring Int64ToString16(std::int64_t value) ;

std::string Uint64ToString(std::uint64_t value) ;
std::wstring Uint64ToString16(std::uint64_t value) ;

std::string SizeTToString(std::size_t value) ;
std::wstring SizeTToString16(std::size_t value) ;

// DoubleToString converts the double to a string format that ignores the
// locale. If you want to use locale specific formatting, use ICU.
std::string DoubleToString(double value) ;

// String -> number conversions ------------------------------------------------

// Perform a best-effort conversion of the input string to a numeric type,
// setting |*output| to the result of the conversion.  Returns true for
// "perfect" conversions; returns false in the following cases:
//  - Overflow. |*output| will be set to the maximum value supported
//    by the data type.
//  - Underflow. |*output| will be set to the minimum value supported
//    by the data type.
//  - Trailing characters in the string after parsing the number.  |*output|
//    will be set to the value of the number that was parsed.
//  - Leading whitespace in the string before parsing the number. |*output| will
//    be set to the value of the number that was parsed.
//  - No characters parseable as a number at the beginning of the string.
//    |*output| will be set to 0.
//  - Empty string.  |*output| will be set to 0.
// WARNING: Will write to |output| even when returning false.
//          Read the comments above carefully.
bool StringToInt16(const std::string& input, std::int16_t* output) ;
bool StringToInt16(const std::wstring& input, std::int16_t* output) ;

bool StringToUint16(const std::string& input, std::uint16_t* output) ;
bool StringToUint16(const std::wstring& input, std::uint16_t* output) ;

bool StringToInt32(const std::string& input, std::int32_t* output) ;
bool StringToInt32(const std::wstring& input, std::int32_t* output) ;

bool StringToUint32(const std::string& input, std::uint32_t* output) ;
bool StringToUint32(const std::wstring& input, std::uint32_t* output) ;

bool StringToInt64(const std::string& input, int64_t* output) ;
bool StringToInt64(const std::wstring& input, int64_t* output) ;

bool StringToUint64(const std::string& input, uint64_t* output) ;
bool StringToUint64(const std::wstring& input, uint64_t* output) ;

bool StringToSizeT(const std::string& input, size_t* output) ;
bool StringToSizeT(const std::wstring& input, size_t* output) ;

// For floating-point conversions, only conversions of input strings in decimal
// form are defined to work.  Behavior with strings representing floating-point
// numbers in hexadecimal, and strings representing non-finite values (such as
// NaN and inf) is undefined.  Otherwise, these behave the same as the integral
// variants.  This expects the input string to NOT be specific to the locale.
// If your input is locale specific, use ICU to read the number.
// WARNING: Will write to |output| even when returning false.
//          Read the comments here and above StringToInt() carefully.
bool StringToDouble(const std::string& input, double* output) ;

// Hex encoding ----------------------------------------------------------------

// Returns a hex string representation of a binary buffer. The returned hex
// string will be in upper case. This function does not check if |size| is
// within reasonable limits since it's written with trusted data in mind.  If
// you suspect that the data you want to format might be large, the absolute
// max size for |size| should be is
//   std::numeric_limits<size_t>::max() / 2
std::string HexEncode(const void* bytes, std::size_t size) ;

// Best effort conversion, see StringToInt above for restrictions.
// Will only successful parse hex values that will fit into |output|, i.e.
// -0x80000000 < |input| < 0x7FFFFFFF.
bool HexStringToInt32(const std::string& input, std::int32_t* output) ;

// Best effort conversion, see StringToInt above for restrictions.
// Will only successful parse hex values that will fit into |output|, i.e.
// 0x00000000 < |input| < 0xFFFFFFFF.
// The string is not required to start with 0x.
bool HexStringToUInt32(const std::string& input, std::uint32_t* output) ;

// Best effort conversion, see StringToInt above for restrictions.
// Will only successful parse hex values that will fit into |output|, i.e.
// -0x8000000000000000 < |input| < 0x7FFFFFFFFFFFFFFF.
bool HexStringToInt64(const std::string& input, std::int64_t* output) ;

// Best effort conversion, see StringToInt above for restrictions.
// Will only successful parse hex values that will fit into |output|, i.e.
// 0x0000000000000000 < |input| < 0xFFFFFFFFFFFFFFFF.
// The string is not required to start with 0x.
bool HexStringToUInt64(const std::string& input, std::uint64_t* output) ;

// Similar to the previous functions, except that output is a vector of bytes.
// |*output| will contain as many bytes as were successfully parsed prior to the
// error.  There is no overflow, but input.size() must be evenly divisible by 2.
// Leading 0x or +/- are not allowed.
bool HexStringToBytes(
    const std::string& input, std::vector<std::uint8_t>* output) ;

#endif  // V8_VM_APPS_UTILS_STRING_NUMBER_CONVERSIONS_H_
