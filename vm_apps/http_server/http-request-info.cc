// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/http-request-info.h"

// Request method names.
const char HttpRequestInfo::kConnectMethod[] = "CONNECT" ;
const char HttpRequestInfo::kDeleteMethod[] = "DELETE" ;
const char HttpRequestInfo::kGetMethod[] = "GET" ;
const char HttpRequestInfo::kHeadMethod[] = "HEAD" ;
const char HttpRequestInfo::kOptionsMethod[] = "OPTIONS" ;
const char HttpRequestInfo::kPostMethod[] = "POST" ;
const char HttpRequestInfo::kPutMethod[] = "PUT" ;
const char HttpRequestInfo::kTraceMethod[] = "TRACE" ;

// Request specific header names.
const char HttpRequestInfo::kAcceptHeader[] = "Accept" ;
const char HttpRequestInfo::kAcceptCharsetHeader[] = "Accept-Charset" ;
const char HttpRequestInfo::kAcceptEncodingHeader[] = "Accept-Encoding" ;
const char HttpRequestInfo::kAcceptLanguageHeader[] = "Accept-Language" ;
const char HttpRequestInfo::kAuthorizationHeader[] = "Authorization" ;
const char HttpRequestInfo::kCookieHeader[] = "Cookie" ;
const char HttpRequestInfo::kExpectHeader[] = "Expect" ;
const char HttpRequestInfo::kFromHeader[] = "From" ;
const char HttpRequestInfo::kHostHeader[] = "Host" ;
const char HttpRequestInfo::kIfMatchHeader[] = "If-Match" ;
const char HttpRequestInfo::kIfModifiedSinceHeader[] = "If-Modified-Since" ;
const char HttpRequestInfo::kIfNoneMatchHeader[] = "If-None-Match" ;
const char HttpRequestInfo::kIfRangeHeader[] = "If-Range" ;
const char HttpRequestInfo::kIfUnmodifiedSinceHeader[] = "If-Unmodified-Since" ;
const char HttpRequestInfo::kMaxForwardsHeader[] = "Max-Forwards" ;
const char HttpRequestInfo::kProxyAuthorizationHeader[] =
    "Proxy-Authorization" ;
const char HttpRequestInfo::kRangeHeader[] = "Range" ;
const char HttpRequestInfo::kRefererHeader[] = "Referer" ;
const char HttpRequestInfo::kTEHeader[] = "TE" ;
const char HttpRequestInfo::kUserAgentHeader[] = "User-Agent" ;

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
      method_.empty() ? kGetMethod : method_.c_str(),
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

  if (EqualsCaseInsensitiveASCII(key, kHostHeader)) {
    if (deleted) {
      host_ = "" ;
    } else {
      host_ = value ;
    }
  }
}
