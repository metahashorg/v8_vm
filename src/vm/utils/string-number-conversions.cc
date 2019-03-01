// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/vm/utils/string-number-conversions.h"

#include <wctype.h>

#include <cctype>
#include <cwctype>
#include <limits>
#include <type_traits>

#include "src/vm/utils/scoped-clear-errno.h"
#include "third_party/numerics/safe_math.h"
#include "third_party/dmg_fp/dmg_fp.h"

namespace {

template <typename STR, typename INT>
struct IntToStringT {
  static STR IntToString(INT value) {
    // log10(2) ~= 0.3 bytes needed per bit or per byte log10(2**8) ~= 2.4.
    // So round up to allocate 3 output characters per byte, plus 1 for '-'.
    const size_t kOutputBufSize =
        3 * sizeof(INT) + std::numeric_limits<INT>::is_signed ;

    // Create the string in a temporary buffer, write it back to front, and
    // then return the substr of what we ended up using.
    using CHR = typename STR::value_type ;
    CHR outbuf[kOutputBufSize] ;

    // The ValueOrDie call below can never fail, because UnsignedAbs is valid
    // for all valid inputs.
    typename std::make_unsigned<INT>::type res =
        chromium::CheckedNumeric<INT>(value).UnsignedAbs().ValueOrDie() ;

    CHR* end = outbuf + kOutputBufSize ;
    CHR* i = end ;
    do {
      --i ;
      // TODO: DCHECK(i != outbuf) ;
      *i = static_cast<CHR>((res % 10) + '0') ;
      res /= 10 ;
    } while (res != 0) ;

    if (chromium::IsValueNegative(value)) {
      --i ;
      // TODO: DCHECK(i != outbuf) ;
      *i = static_cast<CHR>('-') ;
    }

    return STR(i, end) ;
  }
};

// Utility to convert a character to a digit in a given base
template<typename CHAR, int BASE, bool BASE_LTE_10> class BaseCharToDigit {
};

// Faster specialization for bases <= 10
template<typename CHAR, int BASE> class BaseCharToDigit<CHAR, BASE, true> {
 public:
  static bool Convert(CHAR c, uint8_t* digit) {
    if (c >= '0' && c < '0' + BASE) {
      *digit = static_cast<uint8_t>(c - '0') ;
      return true ;
    }

    return false ;
  }
};

// Specialization for bases where 10 < base <= 36
template<typename CHAR, int BASE> class BaseCharToDigit<CHAR, BASE, false> {
 public:
  static bool Convert(CHAR c, uint8_t* digit) {
    if (c >= '0' && c <= '9') {
      *digit = c - '0' ;
    } else if (c >= 'a' && c < 'a' + BASE - 10) {
      *digit = c - 'a' + 10 ;
    } else if (c >= 'A' && c < 'A' + BASE - 10) {
      *digit = c - 'A' + 10 ;
    } else {
      return false ;
    }

    return true ;
  }
};

template <int BASE, typename CHAR>
bool CharToDigit(CHAR c, uint8_t* digit) {
  return BaseCharToDigit<CHAR, BASE, BASE <= 10>::Convert(c, digit) ;
}

// There is an IsUnicodeWhitespace for wchars defined in string_util.h, but it
// is locale independent, whereas the functions we are replacing were
// locale-dependent. TBD what is desired, but for the moment let's not
// introduce a change in behaviour.
template<typename CHAR> class WhitespaceHelper {
};

template<> class WhitespaceHelper<char> {
 public:
  static bool Invoke(char c) {
    return 0 != std::isspace(static_cast<unsigned char>(c)) ;
  }
};

template<> class WhitespaceHelper<wchar_t> {
 public:
  static bool Invoke(wchar_t c) {
    return 0 != std::iswspace(c) ;
  }
};

template<typename CHAR> bool LocalIsWhitespace(CHAR c) {
  return WhitespaceHelper<CHAR>::Invoke(c) ;
}

// IteratorRangeToNumberTraits should provide:
//  - a typedef for iterator_type, the iterator type used as input.
//  - a typedef for value_type, the target numeric type.
//  - static functions min, max (returning the minimum and maximum permitted
//    values)
//  - constant kBase, the base in which to interpret the input
template<typename IteratorRangeToNumberTraits>
class IteratorRangeToNumber {
 public:
  typedef IteratorRangeToNumberTraits traits ;
  typedef typename traits::iterator_type const_iterator ;
  typedef typename traits::value_type value_type ;

  // Generalized iterator-range-to-number conversion.
  //
  static bool Invoke(const_iterator begin,
                     const_iterator end,
                     value_type* output) {
    bool valid = true ;

    while (begin != end && LocalIsWhitespace(*begin)) {
      valid = false ;
      ++begin ;
    }

    if (begin != end && *begin == '-') {
      if (!std::numeric_limits<value_type>::is_signed) {
        *output = 0 ;
        valid = false ;
      } else if (!Negative::Invoke(begin + 1, end, output)) {
        valid = false ;
      }
    } else {
      if (begin != end && *begin == '+') {
        ++begin ;
      }
      if (!Positive::Invoke(begin, end, output)) {
        valid = false ;
      }
    }

    return valid ;
  }

 private:
  // Sign provides:
  //  - a static function, CheckBounds, that determines whether the next digit
  //    causes an overflow/underflow
  //  - a static function, Increment, that appends the next digit appropriately
  //    according to the sign of the number being parsed.
  template<typename Sign>
  class Base {
   public:
    static bool Invoke(const_iterator begin, const_iterator end,
                       typename traits::value_type* output) {
      *output = 0 ;

      if (begin == end) {
        return false ;
      }

      // Note: no performance difference was found when using template
      // specialization to remove this check in bases other than 16
      if (traits::kBase == 16 && end - begin > 2 && *begin == '0' &&
          (*(begin + 1) == 'x' || *(begin + 1) == 'X')) {
        begin += 2 ;
      }

      for (const_iterator current = begin; current != end; ++current) {
        uint8_t new_digit = 0 ;

        if (!CharToDigit<traits::kBase>(*current, &new_digit)) {
          return false ;
        }

        if (current != begin) {
          if (!Sign::CheckBounds(output, new_digit)) {
            return false ;
          }
          *output *= traits::kBase ;
        }

        Sign::Increment(new_digit, output) ;
      }

      return true ;
    }
  };

  class Positive : public Base<Positive> {
   public:
    static bool CheckBounds(value_type* output, uint8_t new_digit) {
      if (*output > static_cast<value_type>(traits::max() / traits::kBase) ||
          (*output == static_cast<value_type>(traits::max() / traits::kBase) &&
           new_digit > traits::max() % traits::kBase)) {
        *output = traits::max() ;
        return false ;
      }

      return true ;
    }

    static void Increment(uint8_t increment, value_type* output) {
      *output += increment ;
    }
  };

  class Negative : public Base<Negative> {
   public:
    static bool CheckBounds(value_type* output, uint8_t new_digit) {
      if (*output < traits::min() / traits::kBase ||
          (*output == traits::min() / traits::kBase &&
           new_digit > 0 - traits::min() % traits::kBase)) {
        *output = traits::min() ;
        return false ;
      }

      return true;
    }

    static void Increment(uint8_t increment, value_type* output) {
      *output -= increment ;
    }
  };
};

template<typename ITERATOR, typename VALUE, int BASE>
class BaseIteratorRangeToNumberTraits {
 public:
  typedef ITERATOR iterator_type ;
  typedef VALUE value_type ;

  static value_type min() {
    return std::numeric_limits<value_type>::min() ;
  }

  static value_type max() {
    return std::numeric_limits<value_type>::max() ;
  }

  static const int kBase = BASE ;
};

template<typename ITERATOR>
class BaseHexIteratorRangeToInt32Traits
    : public BaseIteratorRangeToNumberTraits<ITERATOR, std::int32_t, 16> {
};

template <typename ITERATOR>
class BaseHexIteratorRangeToUInt32Traits
    : public BaseIteratorRangeToNumberTraits<ITERATOR, std::uint32_t, 16> {};

template <typename ITERATOR>
class BaseHexIteratorRangeToInt64Traits
    : public BaseIteratorRangeToNumberTraits<ITERATOR, std::int64_t, 16> {};

template <typename ITERATOR>
class BaseHexIteratorRangeToUInt64Traits
    : public BaseIteratorRangeToNumberTraits<ITERATOR, std::uint64_t, 16> {};

typedef BaseHexIteratorRangeToInt32Traits<std::string::const_iterator>
    HexIteratorRangeToInt32Traits ;

typedef BaseHexIteratorRangeToUInt32Traits<std::string::const_iterator>
    HexIteratorRangeToUInt32Traits ;

typedef BaseHexIteratorRangeToInt64Traits<std::string::const_iterator>
    HexIteratorRangeToInt64Traits ;

typedef BaseHexIteratorRangeToUInt64Traits<std::string::const_iterator>
    HexIteratorRangeToUInt64Traits ;

template <typename STR>
bool HexStringToBytesT(const STR& input, std::vector<uint8_t>* output) {
  // TODO: DCHECK_EQ(output->size(), 0u) ;
  size_t count = input.size() ;
  if (count == 0 || (count % 2) != 0) {
    return false ;
  }

  for (uintptr_t i = 0; i < count / 2; ++i) {
    uint8_t msb = 0 ;  // most significant 4 bits
    uint8_t lsb = 0 ;  // least significant 4 bits
    if (!CharToDigit<16>(input[i * 2], &msb) ||
        !CharToDigit<16>(input[i * 2 + 1], &lsb)) {
      return false ;
    }

    output->push_back((msb << 4) | lsb) ;
  }

  return true ;
}

template <typename VALUE, int BASE>
class StringToNumberTraits
    : public BaseIteratorRangeToNumberTraits<
          std::string::const_iterator, VALUE, BASE> {};

template <typename VALUE>
bool StringToIntImpl(const std::string& input, VALUE* output) {
  return IteratorRangeToNumber<StringToNumberTraits<VALUE, 10> >::Invoke(
      input.begin(), input.end(), output) ;
}

template <typename VALUE, int BASE>
class String16ToNumberTraits
    : public BaseIteratorRangeToNumberTraits<
          std::wstring::const_iterator, VALUE, BASE> {
};

template <typename VALUE>
bool String16ToIntImpl(const std::wstring& input, VALUE* output) {
  return IteratorRangeToNumber<String16ToNumberTraits<VALUE, 10> >::Invoke(
      input.begin(), input.end(), output) ;
}

}  // namespace

namespace v8 {
namespace vm {
namespace internal {

std::string Int32ToString(std::int32_t value) {
  return IntToStringT<std::string, int>::IntToString(value) ;
}

std::wstring Int32ToString16(std::int32_t value) {
  return IntToStringT<std::wstring, int>::IntToString(value) ;
}

std::string UintToString(std::uint32_t value) {
  return IntToStringT<std::string, unsigned int>::IntToString(value) ;
}

std::wstring UintToString16(std::uint32_t value) {
  return IntToStringT<std::wstring, unsigned int>::IntToString(value) ;
}

std::string Int64ToString(int64_t value) {
  return IntToStringT<std::string, int64_t>::IntToString(value) ;
}

std::wstring Int64ToString16(int64_t value) {
  return IntToStringT<std::wstring, int64_t>::IntToString(value) ;
}

std::string Uint64ToString(uint64_t value) {
  return IntToStringT<std::string, uint64_t>::IntToString(value) ;
}

std::wstring Uint64ToString16(uint64_t value) {
  return IntToStringT<std::wstring, uint64_t>::IntToString(value) ;
}

std::string SizeTToString(size_t value) {
  return IntToStringT<std::string, size_t>::IntToString(value) ;
}

std::wstring SizeTToString16(size_t value) {
  return IntToStringT<std::wstring, size_t>::IntToString(value) ;
}

std::string DoubleToString(double value) {
  // According to g_fmt.cc, it is sufficient to declare a buffer of size 32.
  char buffer[32] ;
  dmg_fp::g_fmt(buffer, value) ;
  return std::string(buffer) ;
}

bool StringToInt16(const std::string& input, std::int16_t* output) {
  return StringToIntImpl(input, output) ;
}

bool StringToInt16(const std::wstring& input, std::int16_t* output) {
  return String16ToIntImpl(input, output) ;
}

bool StringToUint16(const std::string& input, std::uint16_t* output) {
  return StringToIntImpl(input, output) ;
}

bool StringToUint16(const std::wstring& input, std::uint16_t* output) {
  return String16ToIntImpl(input, output) ;
}

bool StringToInt32(const std::string& input, std::int32_t* output) {
  return StringToIntImpl(input, output) ;
}

bool StringToInt32(const std::wstring& input, std::int32_t* output) {
  return String16ToIntImpl(input, output) ;
}

bool StringToUint32(const std::string& input, std::uint32_t* output) {
  return StringToIntImpl(input, output) ;
}

bool StringToUint32(const std::wstring& input, std::uint32_t* output) {
  return String16ToIntImpl(input, output) ;
}

bool StringToInt64(const std::string& input, std::int64_t* output) {
  return StringToIntImpl(input, output) ;
}

bool StringToInt64(const std::wstring& input, std::int64_t* output) {
  return String16ToIntImpl(input, output) ;
}

bool StringToUint64(const std::string& input, std::uint64_t* output) {
  return StringToIntImpl(input, output) ;
}

bool StringToUint64(const std::wstring& input, std::uint64_t* output) {
  return String16ToIntImpl(input, output) ;
}

bool StringToSizeT(const std::string& input, std::size_t* output) {
  return StringToIntImpl(input, output) ;
}

bool StringToSizeT(const std::wstring& input, std::size_t* output) {
  return String16ToIntImpl(input, output) ;
}

bool StringToDouble(const std::string& input, double* output) {
  // Thread-safe?  It is on at least Mac, Linux, and Windows.
  ScopedClearErrno clear_errno ;

  char* endptr = NULL ;
  *output = dmg_fp::strtod(input.c_str(), &endptr) ;

  // Cases to return false:
  //  - If errno is ERANGE, there was an overflow or underflow.
  //  - If the input string is empty, there was nothing to parse.
  //  - If endptr does not point to the end of the string, there are either
  //    characters remaining in the string after a parsed number, or the string
  //    does not begin with a parseable number.  endptr is compared to the
  //    expected end given the string's stated length to correctly catch cases
  //    where the string contains embedded NUL characters.
  //  - If the first character is a space, there was leading whitespace
  return errno == 0 &&
         !input.empty() &&
         input.c_str() + input.length() == endptr &&
         !isspace(input[0]) ;
}

// Note: if you need to add String16ToDouble, first ask yourself if it's
// really necessary. If it is, probably the best implementation here is to
// convert to 8-bit and then use the 8-bit version.

// Note: if you need to add an iterator range version of StringToDouble, first
// ask yourself if it's really necessary. If it is, probably the best
// implementation here is to instantiate a string and use the string version.

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

bool HexStringToInt32(const std::string& input, std::int32_t* output) {
  return IteratorRangeToNumber<HexIteratorRangeToInt32Traits>::Invoke(
    input.begin(), input.end(), output) ;
}

bool HexStringToUInt32(const std::string& input, uint32_t* output) {
  return IteratorRangeToNumber<HexIteratorRangeToUInt32Traits>::Invoke(
      input.begin(), input.end(), output) ;
}

bool HexStringToInt64(const std::string& input, int64_t* output) {
  return IteratorRangeToNumber<HexIteratorRangeToInt64Traits>::Invoke(
    input.begin(), input.end(), output) ;
}

bool HexStringToUInt64(const std::string& input, uint64_t* output) {
  return IteratorRangeToNumber<HexIteratorRangeToUInt64Traits>::Invoke(
      input.begin(), input.end(), output) ;
}

bool HexStringToBytes(const std::string& input, std::vector<uint8_t>* output) {
  return HexStringToBytesT(input, output) ;
}

}  // namespace internal
}  // namespace vm
}  // namespace v8
