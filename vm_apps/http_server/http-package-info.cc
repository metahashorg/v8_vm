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
const char HttpPackageInfo::Header::kCacheControl[] = "Cache-Control" ;
const char HttpPackageInfo::Header::kConnection[] = "Connection" ;
const char HttpPackageInfo::Header::kDate[] = "Date" ;
const char HttpPackageInfo::Header::kPragma[] = "Pragma" ;
const char HttpPackageInfo::Header::kTrailer[] = "Trailer" ;
const char HttpPackageInfo::Header::kTransferEncoding[] = "Transfer-Encoding" ;
const char HttpPackageInfo::Header::kUpgrade[] = "Upgrade" ;
const char HttpPackageInfo::Header::kVia[] = "Via" ;
const char HttpPackageInfo::Header::kWarning[] = "Warning" ;

// Entity header names.
const char HttpPackageInfo::Header::kAllow[] = "Allow" ;
const char HttpPackageInfo::Header::kContentEncoding[] = "Content-Encoding" ;
const char HttpPackageInfo::Header::kContentLanguage[] = "Content-Language" ;
const char HttpPackageInfo::Header::kContentLength[] = "Content-Length" ;
const char HttpPackageInfo::Header::kContentLocation[] = "Content-Location" ;
const char HttpPackageInfo::Header::kContentMD5[] = "Content-MD5" ;
const char HttpPackageInfo::Header::kContentRange[] = "Content-Range" ;
const char HttpPackageInfo::Header::kContentType[] = "Content-Type" ;
const char HttpPackageInfo::Header::kExpires[] = "Expires" ;
const char HttpPackageInfo::Header::kLastModified[] = "Last-Modified" ;

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
  raw_headers_error_ = vv::errObjNotInit ;

  if (body_ && body_owned_) {
    delete [] body_ ;
  }

  body_ = nullptr ;
  content_length_ = -1 ;
  body_size_ = 0 ;
  body_owned_ = false ;
  body_error_ = vv::wrnObjNotInit ;
  body_getter_ = nullptr ;
}

vv::Error HttpPackageInfo::Parse(
    const char* data, std::int32_t size, bool owned) {
  Clear() ;
  return ParseImpl(data, size, owned) ;
}

vv::Error HttpPackageInfo::ParseHttpVersion(
    const char* str_begin, const char* str_end) {
  // RFC2616 sec 3.1: HTTP-Version   = "HTTP" "/" 1*DIGIT "." 1*DIGIT
  const char* p = str_begin ;
  if ((str_end - str_begin) < 8 ||
      !EqualsCaseInsensitiveASCII(std::string(str_begin, 4), "http")) {
    printf("ERROR: ParseHttpVersion is failed (Line:%d)\n", __LINE__) ;
    return vv::errInvalidArgument ;
  }

  p += 4 ;

  if (*p != '/') {
    printf("ERROR: HTTP version is omitted (Line:%d)\n", __LINE__) ;
    return vv::errInvalidArgument ;
  }

  const char* dot = std::find(p, str_end, '.') ;
  if (dot == str_end) {
    printf("ERROR: HTTP version is malformed (Line:%d)\n", __LINE__) ;
    return vv::errInvalidArgument ;
  }

  ++p ;  // from / to first digit.
  ++dot ;  // from . to second digit.

  if (!std::isdigit(*p) || !std::isdigit(*dot)) {
    printf("ERROR: HTTP version is malformed (Line:%d)\n", __LINE__) ;
    return vv::errInvalidArgument ;
  }

  uint16_t major = *p - '0' ;
  uint16_t minor = *dot - '0' ;

  SetHttpVersion(HttpVersion(major, minor)) ;
  return vv::errOk ;
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
    printf("WARN: Try to set a corrupted HTTP version (HTTP/%d.%d)\n",
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

vv::Error HttpPackageInfo::SetHeader(
    const std::string& key, const std::string& value) {
  // TODO:
  // DCHECK(IsValidHeaderName(key)) ;
  // DCHECK(IsValidHeaderValue(value)) ;

  if (!IsValidHeaderName(key) || !IsValidHeaderValue(value)) {
    printf("ERROR: SetHeader is failed (key: \'%s\' value: \'%s\')\n",
           key.c_str(), value.c_str()) ;
    return vv::errInvalidArgument ;
  }

  HeaderVector::iterator it = FindHeader(key) ;
  if (it != headers_.end()) {
    it->value.assign(value.data(), value.size()) ;
  } else {
    headers_.push_back(HeaderKeyValuePair(key, value)) ;
  }

  UpdateInfoByHeader(key, value) ;
  return vv::errOk ;
}

vv::Error HttpPackageInfo::SetHeaderIfMissing(
    const std::string& key, const std::string& value) {
  // TODO:
  // DCHECK(IsValidHeaderName(key)) ;
  // DCHECK(IsValidHeaderValue(value)) ;

  if (!IsValidHeaderName(key) || !IsValidHeaderValue(value)) {
    printf("ERROR: SetHeaderIfMissing is failed (key: \'%s\' value: \'%s\')\n",
           key.c_str(), value.c_str()) ;
    return vv::errInvalidArgument ;
  }

  HeaderVector::iterator it = FindHeader(key) ;
  if (it == headers_.end()) {
    headers_.push_back(HeaderKeyValuePair(key, value)) ;
    UpdateInfoByHeader(key, value) ;
  }

  return vv::errOk ;
}

vv::Error HttpPackageInfo::GetBody(const char*& body, std::int32_t& body_size) {
  // TODO: DCHECK(body_error_ != vv::wrnObjNotInit || body_getter_) ;

  if (body_error_ == vv::wrnObjNotInit && body_getter_) {
    const char* local_body = nullptr ;
    std::int32_t local_body_size = 0 ;
    bool local_body_owned = 0 ;
    vv::Error local_body_error = body_getter_(
        local_body, local_body_size, local_body_owned) ;
    if (V8_ERR_SUCCESSED(body_error_)) {
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
  body_ = body ;
  body_size_ = body_size ;
  body_owned_ = owned ;
  body_error_ = vv::errOk ;

  if (body_size >= 0) {
    SetHeader(Header::kContentLength, StringPrintf("%d", body_size_)) ;
  } else {
    RemoveHeader(Header::kContentLength) ;
  }
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

vv::Error HttpPackageInfo::ParseImpl(
    const char* headers, std::int32_t size, bool owned) {
  raw_headers_ = headers ;
  raw_headers_size_ = size ;
  raw_headers_owned_ = owned ;
  raw_headers_error_ = vv::errOk ;

  // Ensure the headers end with '\r\n'.
  if (raw_headers_size_ < 2 ||
      raw_headers_[raw_headers_size_ - 2] != '\r' ||
      raw_headers_[raw_headers_size_ - 1] != '\n') {
    printf("ERROR: HttpPackageInfo::Parse is failed\n") ;
    return vv::errInvalidArgument ;
  }

  vv::Error result = vv::errOk ;
  std::string headers_str(raw_headers_, raw_headers_size_) ;
  HeadersIterator headers_it(
      headers_str.begin(), headers_str.end(), std::string("\r\n")) ;
  while (headers_it.GetNext()) {
    result = SetHeader(
        std::string(headers_it.name_begin(), headers_it.name_end()),
        std::string(headers_it.values_begin(), headers_it.values_end())) ;
    if (V8_ERR_FAILED(raw_headers_error_)) {
      printf("ERROR: HttpPackageInfo::Parse is failed on SetHeader()\n") ;
      break ;
    }
  }

  return result ;
}

void HttpPackageInfo::UpdateInfoByHeader(
    const std::string& key, const std::string& value, bool deleted) {
  if (EqualsCaseInsensitiveASCII(key, Header::kContentLength)) {
    if (!deleted && !value.empty() && std::isdigit(value[0])) {
      content_length_ = StringToInt32(value.c_str()) ;
    } else {
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
