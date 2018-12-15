// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/http-request-info.h"

// Request method names.
const char HttpRequestInfo::Method::kConnect[] = "CONNECT" ;
const char HttpRequestInfo::Method::kDelete[] = "DELETE" ;
const char HttpRequestInfo::Method::kGet[] = "GET" ;
const char HttpRequestInfo::Method::kHead[] = "HEAD" ;
const char HttpRequestInfo::Method::kOptions[] = "OPTIONS" ;
const char HttpRequestInfo::Method::kPost[] = "POST" ;
const char HttpRequestInfo::Method::kPut[] = "PUT" ;
const char HttpRequestInfo::Method::kTrace[] = "TRACE" ;

// Request specific header names.
const char HttpRequestInfo::Header::kAccept[] = "Accept" ;
const char HttpRequestInfo::Header::kAcceptCharset[] = "Accept-Charset" ;
const char HttpRequestInfo::Header::kAcceptEncoding[] = "Accept-Encoding" ;
const char HttpRequestInfo::Header::kAcceptLanguage[] = "Accept-Language" ;
const char HttpRequestInfo::Header::kAuthorization[] = "Authorization" ;
const char HttpRequestInfo::Header::kCookie[] = "Cookie" ;
const char HttpRequestInfo::Header::kExpect[] = "Expect" ;
const char HttpRequestInfo::Header::kFrom[] = "From" ;
const char HttpRequestInfo::Header::kHost[] = "Host" ;
const char HttpRequestInfo::Header::kIfMatch[] = "If-Match" ;
const char HttpRequestInfo::Header::kIfModifiedSince[] = "If-Modified-Since" ;
const char HttpRequestInfo::Header::kIfNoneMatch[] = "If-None-Match" ;
const char HttpRequestInfo::Header::kIfRange[] = "If-Range" ;
const char HttpRequestInfo::Header::kIfUnmodifiedSince[] = "If-Unmodified-Since" ;
const char HttpRequestInfo::Header::kMaxForwards[] = "Max-Forwards" ;
const char HttpRequestInfo::Header::kProxyAuthorization[] =
    "Proxy-Authorization" ;
const char HttpRequestInfo::Header::kRange[] = "Range" ;
const char HttpRequestInfo::Header::kReferer[] = "Referer" ;
const char HttpRequestInfo::Header::kTE[] = "TE" ;
const char HttpRequestInfo::Header::kUserAgent[] = "User-Agent" ;

HttpRequestInfo::HttpRequestInfo() {}

HttpRequestInfo::~HttpRequestInfo() {}

void HttpRequestInfo::Clear() {
  method_ = "" ;
  uri_ = "" ;
  host_ = "" ;

  if (raw_request_ && raw_request_owned_) {
    delete [] raw_request_ ;
  }

  raw_request_ = nullptr ;
  raw_request_size_ = 0 ;
  raw_request_owned_ = false ;
  raw_request_error_ = vv::errObjNotInit ;

  HttpPackageInfo::Clear() ;
}

std::string HttpRequestInfo::ToString() const {
  std::string output = StringPrintf(
      "%s %s HTTP/%d.%d\r\n",
      method_.empty() ? Method::kGet : method_.c_str(),
      uri_.empty() ? "/" : uri_.c_str(),
      http_version().major_value(), http_version().minor_value()) ;
  output += HttpPackageInfo::ToString() ;
  return output ;
}

vv::Error HttpRequestInfo::ParseImpl(
    const char* request, std::int32_t size, bool owned) {
  raw_request_ = request ;
  raw_request_size_ = size ;
  raw_request_owned_ = owned ;
  raw_request_error_ = vv::errOk ;

  // Find http-headers
  const char* headers = std::find(request, request + size, '\r') ;
  if ((request + size - headers) < 2 || headers[1] != '\n') {
    printf("ERROR: HttpRequestInfo::Parse is failed (Line:%d)\n", __LINE__) ;
    return vv::errInvalidArgument ;
  }

  headers += 2 ;

  // Read a method name
  const char* method_end = std::find(request, headers, ' ') ;
  if (method_end == headers) {
    printf("ERROR: HttpRequestInfo::Parse is failed (Line:%d)\n", __LINE__) ;
    return vv::errInvalidArgument ;
  }

  method_.assign(request, method_end) ;
  if (!IsToken(method_)) {
    printf("ERROR: HttpRequestInfo::Parse is failed (Line:%d)\n", __LINE__) ;
    return vv::errInvalidArgument ;
  }

  // Read URI
  request = method_end ;
  // Skip whitespace.
  while (request < headers && *request == ' ') {
    ++request ;
  }

  const char* uri_end = std::find(request, headers, ' ') ;
  if (uri_end == headers) {
    printf("ERROR: HttpRequestInfo::Parse is failed (Line:%d)\n", __LINE__) ;
    return vv::errInvalidArgument ;
  }

  uri_.assign(request, uri_end) ;

  // Read http-version
  request = uri_end ;
  // Skip whitespace.
  while (request < headers && *request == ' ') {
    ++request ;
  }

  vv::Error result = ParseHttpVersion(request, headers) ;
  if (V8_ERR_FAILED(result)) {
    printf("ERROR: HttpRequestInfo::Parse is failed (Line:%d)\n", __LINE__) ;
    return result ;
  }

  // Parse http-headers
  std::int32_t headers_size =
      size - static_cast<std::int32_t>(headers - raw_request_) ;
  result = HttpPackageInfo::ParseImpl(headers, headers_size, false) ;
  if (V8_ERR_FAILED(result)) {
    printf("ERROR: HttpRequestInfo::Parse is failed (Line:%d)\n", __LINE__) ;
    return result ;
  }

  return result ;
}

void HttpRequestInfo::UpdateInfoByHeader(
    const std::string& key, const std::string& value, bool deleted) {
  HttpPackageInfo::UpdateInfoByHeader(key, value, deleted) ;

  if (EqualsCaseInsensitiveASCII(key, Header::kHost)) {
    if (deleted) {
      host_ = "" ;
    } else {
      host_ = value ;
    }
  }
}
