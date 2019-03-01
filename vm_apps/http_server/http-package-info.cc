// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/http-package-info.h"

#include <cctype>

#include "vm_apps/utils/app-utils.h"
#include "vm_apps/utils/string-tokenizer.h"

#define HTTP_LWS " \t"

namespace {

bool IsTokenChar(char c) {
  return !(c >= 0x7F || c <= 0x20 || c == '(' || c == ')' || c == '<' ||
           c == '>' || c == '@' || c == ',' || c == ';' || c == ':' ||
           c == '\\' || c == '"' || c == '/' || c == '[' || c == ']' ||
           c == '?' || c == '=' || c == '{' || c == '}') ;
}

bool IsValidHeaderName(const std::string& name) {
  // Check whether the header name is RFC 2616-compliant.
  return HttpPackageInfo::IsToken(name);
}

bool IsValidHeaderValue(const std::string& value) {
  // Just a sanity check: disallow NULL, CR and LF.
  for (char c : value) {
    if (c == '\0' || c == '\r' || c == '\n') {
      return false ;
    }
  }

  return true ;
}

bool IsLWS(char c) {
  static const char lws[] = HTTP_LWS ;
  for (const char* cstr = lws; *cstr != '\0'; ++cstr) {
    if (c == *cstr) {
      return true ;
    }
  }

  return false ;
}

void TrimLWS(
    std::string::const_iterator* begin, std::string::const_iterator* end) {
  // leading whitespace
  while (*begin < *end && IsLWS((*begin)[0])) {
    ++(*begin) ;
  }

  // trailing whitespace
  while (*begin < *end && IsLWS((*end)[-1])) {
    --(*end) ;
  }
}

// Used to iterate over the name/value pairs of HTTP headers.  To iterate
// over the values in a multi-value header, use ValuesIterator.
// See AssembleRawHeaders for joining line continuations (this iterator
// does not expect any).
class HeadersIterator {
 public:
  HeadersIterator(std::string::const_iterator headers_begin,
                  std::string::const_iterator headers_end,
                  const std::string& line_delimiter) ;
  ~HeadersIterator() ;

  // Advances the iterator to the next header, if any.  Returns true if there
  // is a next header.  Use name* and values* methods to access the resultant
  // header name and values.
  bool GetNext() ;

  void Reset() {
    lines_.Reset() ;
  }

  std::string::const_iterator name_begin() const {
    return name_begin_ ;
  }
  std::string::const_iterator name_end() const {
    return name_end_ ;
  }
  std::string name() const {
    return std::string(name_begin_, name_end_) ;
  }

  std::string::const_iterator values_begin() const {
    return values_begin_ ;
  }
  std::string::const_iterator values_end() const {
    return values_end_ ;
  }
  std::string values() const {
    return std::string(values_begin_, values_end_) ;
  }

 private:
  StringTokenizer lines_ ;
  std::string::const_iterator name_begin_ ;
  std::string::const_iterator name_end_ ;
  std::string::const_iterator values_begin_ ;
  std::string::const_iterator values_end_ ;
};

// BNF from section 4.2 of RFC 2616:
//
//   message-header = field-name ":" [ field-value ]
//   field-name     = token
//   field-value    = *( field-content | LWS )
//   field-content  = <the OCTETs making up the field-value
//                     and consisting of either *TEXT or combinations
//                     of token, separators, and quoted-string>
//

HeadersIterator::HeadersIterator(
    std::string::const_iterator headers_begin,
    std::string::const_iterator headers_end,
    const std::string& line_delimiter)
    : lines_(headers_begin, headers_end, line_delimiter) {
}

HeadersIterator::~HeadersIterator() {
}

bool HeadersIterator::GetNext() {
  while (lines_.GetNext()) {
    name_begin_ = lines_.token_begin() ;
    values_end_ = lines_.token_end() ;

    std::string::const_iterator colon(
        std::find(name_begin_, values_end_, ':')) ;
    if (colon == values_end_) {
      continue ;  // skip malformed header
    }

    name_end_ = colon ;

    // If the name starts with LWS, it is an invalid line.
    // Leading LWS implies a line continuation, and these should have
    // already been joined by AssembleRawHeaders().
    if (name_begin_ == name_end_ || IsLWS(*name_begin_)) {
      continue ;
    }

    TrimLWS(&name_begin_, &name_end_) ;
    // TODO: DCHECK(name_begin_ < name_end_) ;
    if (!HttpPackageInfo::IsToken(std::string(name_begin_, name_end_))) {
      continue ;  // skip malformed header
    }

    values_begin_ = colon + 1 ;
    TrimLWS(&values_begin_, &values_end_) ;

    // if we got a header name, then we are done.
    return true ;
  }

  return false ;
}

}  // namespace

// General header names.
const char HttpPackageInfo::Header::CacheControl[] = "Cache-Control" ;
const char HttpPackageInfo::Header::Connection[] = "Connection" ;
const char HttpPackageInfo::Header::Date[] = "Date" ;
const char HttpPackageInfo::Header::Pragma[] = "Pragma" ;
const char HttpPackageInfo::Header::Trailer[] = "Trailer" ;
const char HttpPackageInfo::Header::TransferEncoding[] = "Transfer-Encoding" ;
const char HttpPackageInfo::Header::Upgrade[] = "Upgrade" ;
const char HttpPackageInfo::Header::Via[] = "Via" ;
const char HttpPackageInfo::Header::Warning[] = "Warning" ;

// Entity header names.
const char HttpPackageInfo::Header::Allow[] = "Allow" ;
const char HttpPackageInfo::Header::ContentEncoding[] = "Content-Encoding" ;
const char HttpPackageInfo::Header::ContentLanguage[] = "Content-Language" ;
const char HttpPackageInfo::Header::ContentLength[] = "Content-Length" ;
const char HttpPackageInfo::Header::ContentLocation[] = "Content-Location" ;
const char HttpPackageInfo::Header::ContentMD5[] = "Content-MD5" ;
const char HttpPackageInfo::Header::ContentRange[] = "Content-Range" ;
const char HttpPackageInfo::Header::ContentType[] = "Content-Type" ;
const char HttpPackageInfo::Header::Expires[] = "Expires" ;
const char HttpPackageInfo::Header::LastModified[] = "Last-Modified" ;

HttpPackageInfo::HeaderKeyValuePair::HeaderKeyValuePair() {
}

HttpPackageInfo::HeaderKeyValuePair::HeaderKeyValuePair(
    const std::string& key, const std::string& value)
    : key(key.data(), key.size()), value(value.data(), value.size()) {
}

HttpPackageInfo::HttpPackageInfo() {}

HttpPackageInfo::~HttpPackageInfo() {
  Clear() ;
}

void HttpPackageInfo::Clear() {
  headers_.clear() ;

  if (raw_headers_ && raw_headers_owned_) {
    delete [] raw_headers_ ;
  }

  raw_headers_ = nullptr ;
  raw_headers_size_ = 0 ;
  raw_headers_owned_ = false ;
  raw_headers_error_ = errObjNotInit ;

  if (body_ && body_owned_) {
    delete [] body_ ;
  }

  body_ = nullptr ;
  body_str_.reset() ;
  content_length_ = -1 ;
  body_size_ = 0 ;
  body_owned_ = false ;
  body_error_ = wrnObjNotInit ;
  body_getter_ = nullptr ;
}

Error HttpPackageInfo::Parse(const char* data, std::int32_t size, bool owned) {
  Clear() ;
  return ParseInternal(data, size, owned) ;
}

Error HttpPackageInfo::ParseHttpVersion(
    const char* str_begin, const char* str_end) {
  // RFC2616 sec 3.1: HTTP-Version   = "HTTP" "/" 1*DIGIT "." 1*DIGIT
  const char* p = str_begin ;
  if ((str_end - str_begin) < 8 ||
      !EqualsCaseInsensitiveASCII(std::string(str_begin, 4), "http")) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        errInvalidArgument, "HTTP version is invalid - \'%s\'",
        std::string(str_begin, str_end).c_str()) ;
  }

  p += 4 ;

  if (*p != '/') {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        errInvalidArgument, "HTTP version is omitted - \'%s\'",
        std::string(str_begin, str_end).c_str()) ;
  }

  const char* dot = std::find(p, str_end, '.') ;
  if (dot == str_end) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        errInvalidArgument, "HTTP version is malformed - \'%s\'",
        std::string(str_begin, str_end).c_str()) ;
  }

  ++p ;  // from / to first digit.
  ++dot ;  // from . to second digit.

  if (!std::isdigit(*p) || !std::isdigit(*dot)) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        errInvalidArgument, "HTTP version is malformed - \'%s\'",
        std::string(str_begin, str_end).c_str()) ;
  }

  uint16_t major = *p - '0' ;
  uint16_t minor = *dot - '0' ;

  SetHttpVersion(HttpVersion(major, minor)) ;
  return errOk ;
}

void HttpPackageInfo::SetHttpVersion(const HttpVersion& http_version) {
  // Clamp the version number to one of: {0.9, 1.0, 1.1, 2.0}
  if (http_version == HttpVersion(0, 9)) {
    http_version_ = HttpVersion(0, 9);
  } else if (http_version == HttpVersion(2, 0)) {
    http_version_ = HttpVersion(2, 0);
  } else if (http_version >= HttpVersion(1, 1)) {
    http_version_ = HttpVersion(1, 1);
  } else {
    // Treat everything else like HTTP 1.0
    http_version_ = HttpVersion(1, 0);
  }

  if (http_version != http_version_) {
    // TODO: DCHECK(http_version == http_version_) ;
    V8_LOG_WRN(
        wrnInvalidArgument, "Try to set a corrupted HTTP version (HTTP/%d.%d)",
        http_version.major_value(), http_version.minor_value()) ;
  }
}

bool HttpPackageInfo::GetHeader(
    const std::string& key, std::string& out) const {
  HeaderVector::const_iterator it = FindHeader(key) ;
  if (it == headers_.end()) {
    return false ;
  }

  out.assign(it->value) ;
  return true ;
}

void HttpPackageInfo::RemoveHeader(const std::string& key) {
  HeaderVector::iterator it = FindHeader(key) ;
  if (it != headers_.end()) {
    headers_.erase(it) ;
    UpdateInfoByHeader(key, "", true) ;
  }
}

Error HttpPackageInfo::SetHeader(
    const std::string& key, const std::string& value) {
  // TODO:
  // DCHECK(IsValidHeaderName(key)) ;
  // DCHECK(IsValidHeaderValue(value)) ;

  if (!IsValidHeaderName(key) || !IsValidHeaderValue(value)) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        errInvalidArgument, "SetHeader() is failed (key: \'%s\' value: \'%s\')",
        key.c_str(), value.c_str()) ;
  }

  HeaderVector::iterator it = FindHeader(key) ;
  if (it != headers_.end()) {
    it->value.assign(value.data(), value.size()) ;
  } else {
    headers_.push_back(HeaderKeyValuePair(key, value)) ;
  }

  UpdateInfoByHeader(key, value) ;
  return errOk ;
}

Error HttpPackageInfo::SetHeaderIfMissing(
    const std::string& key, const std::string& value) {
  // TODO:
  // DCHECK(IsValidHeaderName(key)) ;
  // DCHECK(IsValidHeaderValue(value)) ;

  if (!IsValidHeaderName(key) || !IsValidHeaderValue(value)) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        errInvalidArgument,
        "SetHeaderIfMissing() is failed (key: \'%s\' value: \'%s\')",
        key.c_str(), value.c_str()) ;
  }

  HeaderVector::iterator it = FindHeader(key) ;
  if (it == headers_.end()) {
    headers_.push_back(HeaderKeyValuePair(key, value)) ;
    UpdateInfoByHeader(key, value) ;
  }

  return errOk ;
}

Error HttpPackageInfo::GetBody(const char*& body, std::int32_t& body_size) {
  // TODO: DCHECK(body_error_ != wrnObjNotInit || body_getter_) ;

  if (body_error_ == wrnObjNotInit && body_getter_) {
    const char* local_body = nullptr ;
    std::int32_t local_body_size = 0 ;
    bool local_body_owned = 0 ;
    Error local_body_error = body_getter_(
        local_body, local_body_size, local_body_owned) ;
    if (V8_ERROR_SUCCESS(body_error_)) {
      SetBody(local_body, local_body_size, local_body_owned) ;
    } else {
      SetBody(nullptr, -1, false) ;
    }

    body_error_ = local_body_error ;
  }

  body = body_ ;
  body_size = body_size_ ;
  return body_error_ ;
}

void HttpPackageInfo::SetBody(
    const char* body, std::int32_t body_size, bool owned) {
  body_str_.reset() ;
  SetBodyInternal(body, body_size, owned) ;
}

void HttpPackageInfo::SetBody(const std::string& body) {
  if (body_str_) {
    *body_str_ = body ;
  } else {
    body_str_.reset(new std::string(body)) ;
  }

  SetBodyInternal(
      body_str_->c_str(), static_cast<std::int32_t>(body_str_->length()),
      false) ;
}

void HttpPackageInfo::SetBody(BodyGetter getter) {
  body_getter_ = getter ;
}

std::string HttpPackageInfo::ToString() const {
  std::string output ;
  for (HeaderVector::const_iterator it = headers_.begin();
       it != headers_.end(); ++it) {
    if (!it->value.empty()) {
      StringAppendF(&output, "%s: %s\r\n", it->key.c_str(), it->value.c_str()) ;
    } else {
      StringAppendF(&output, "%s:\r\n", it->key.c_str()) ;
    }
  }

  output.append("\r\n") ;
  return output ;
}

Error HttpPackageInfo::ParseInternal(
    const char* headers, std::int32_t size, bool owned) {
  raw_headers_ = headers ;
  raw_headers_size_ = size ;
  raw_headers_owned_ = owned ;
  raw_headers_error_ = errOk ;

  // Ensure the headers end with '\r\n'.
  if (raw_headers_size_ < 2 ||
      raw_headers_[raw_headers_size_ - 2] != '\r' ||
      raw_headers_[raw_headers_size_ - 1] != '\n') {
    return V8_ERROR_CREATE_WITH_MSG(
        errInvalidArgument, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  std::string headers_str(raw_headers_, raw_headers_size_) ;
  HeadersIterator headers_it(
      headers_str.begin(), headers_str.end(), std::string("\r\n")) ;
  while (headers_it.GetNext()) {
    raw_headers_error_ = SetHeader(
        std::string(headers_it.name_begin(), headers_it.name_end()),
        std::string(headers_it.values_begin(), headers_it.values_end())) ;
    if (V8_ERROR_FAILED(raw_headers_error_)) {
      V8_ERROR_ADD_MSG(raw_headers_error_, V8_ERROR_MSG_FUNCTION_FAILED()) ;
      break ;
    }
  }

  return raw_headers_error_ ;
}

void HttpPackageInfo::UpdateInfoByHeader(
    const std::string& key, const std::string& value, bool deleted) {
  if (EqualsCaseInsensitiveASCII(key, Header::ContentLength)) {
    if (deleted || value.empty() || !std::isdigit(value[0]) ||
        !StringToInt32(value.c_str(), &content_length_)) {
      content_length_ = -1 ;
    }
  }
}

bool HttpPackageInfo::IsToken(const std::string& string) {
  if (string.empty()) {
    return false ;
  }

  for (char c : string) {
    if (!IsTokenChar(c)) {
      return false ;
    }
  }

  return true ;
}

HttpPackageInfo::HeaderVector::iterator
HttpPackageInfo::FindHeader(const std::string& key) {
  for (HeaderVector::iterator it = headers_.begin();
       it != headers_.end(); ++it) {
    if (EqualsCaseInsensitiveASCII(key, it->key)) {
      return it ;
    }
  }

  return headers_.end() ;
}

HttpPackageInfo::HeaderVector::const_iterator
HttpPackageInfo::FindHeader(const std::string& key) const {
  for (HeaderVector::const_iterator it = headers_.begin();
       it != headers_.end(); ++it) {
    if (EqualsCaseInsensitiveASCII(key, it->key)) {
      return it ;
    }
  }

  return headers_.end() ;
}

void HttpPackageInfo::SetBodyInternal(
    const char* body, std::int32_t body_size, bool owned) {
  if (body_ && body_owned_) {
    delete [] body_ ;
  }

  body_ = body ;
  body_size_ = body_size ;
  body_owned_ = owned ;
  body_error_ = errOk ;

  if (body_size >= 0) {
    SetHeader(Header::ContentLength, StringPrintf("%d", body_size_)) ;
  } else {
    RemoveHeader(Header::ContentLength) ;
  }
}
