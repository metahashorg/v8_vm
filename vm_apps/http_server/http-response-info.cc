// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/http-response-info.h"

#include <cctype>

#include "src/vm/utils/string-printf.h"
#include "vm_apps/utils/string-number-conversions.h"

// Response specific header names.
const char HttpResponseInfo::Header::AcceptRanges[] = "Accept-Ranges" ;
const char HttpResponseInfo::Header::Age[] = "Age" ;
const char HttpResponseInfo::Header::ETag[] = "ETag" ;
const char HttpResponseInfo::Header::Location[] = "Location" ;
const char HttpResponseInfo::Header::ProxyAuthenticate[] =
    "Proxy-Authenticate" ;
const char HttpResponseInfo::Header::RetryAfter[] = "Retry-After" ;
const char HttpResponseInfo::Header::Server[] = "Server" ;
const char HttpResponseInfo::Header::SetCookie[] = "Set-Cookie" ;
const char HttpResponseInfo::Header::Vary[] = "Vary" ;
const char HttpResponseInfo::Header::WWWAuthenticate[] = "WWW-Authenticate" ;

HttpResponseInfo::HttpResponseInfo() {}

HttpResponseInfo::HttpResponseInfo(std::int32_t status_code) {
  SetStatusCode(status_code) ;
}

HttpResponseInfo::~HttpResponseInfo() {}

void HttpResponseInfo::Clear() {
  status_code_ = HTTP_OK ;

  if (raw_response_ && raw_response_owned_) {
    delete [] raw_response_ ;
  }

  raw_response_ = nullptr ;
  raw_response_size_ = 0 ;
  raw_response_owned_ = false ;
  raw_response_error_ = vv::errObjNotInit ;

  HttpPackageInfo::Clear() ;
}

vv::Error HttpResponseInfo::SetStatusCode(std::int32_t status_code) {
  if (status_code < 100 || status_code > 599) {
    printf("ERROR: SetStatusCode is failed.\n") ;
    return vv::errInvalidArgument ;
  }

  status_code_ = static_cast<HttpStatusCode>(status_code) ;
  return vv::errOk ;
}

std::string HttpResponseInfo::ToString() const {
  std::string output = vvi::StringPrintf(
      "HTTP/%d.%d %d %s\r\n",
      http_version().major_value(), http_version().minor_value(),
      status_code_, GetHttpReasonPhrase(status_code_)) ;
  output += HttpPackageInfo::ToString() ;
  return output ;
}

vv::Error HttpResponseInfo::ParseInternal(
    const char* response, std::int32_t size, bool owned) {
  raw_response_ = response ;
  raw_response_size_ = size ;
  raw_response_owned_ = owned ;
  raw_response_error_ = vv::errOk ;

  // Find http-headers
  const char* headers = std::find(response, response + size, '\r') ;
  if ((response + size - headers) < 2 || headers[1] != '\n') {
    printf("ERROR: HttpRequestInfo::Parse is failed (Line:%d)\n", __LINE__) ;
    return vv::errInvalidArgument ;
  }

  headers += 2 ;

  // Read http-version
  vv::Error result = ParseHttpVersion(response, headers) ;
  if (V8_ERR_FAILED(result)) {
    printf("ERROR: HttpRequestInfo::Parse is failed (Line:%d)\n", __LINE__) ;
  }

  // Read status code
  response = std::find(response, headers, ' ') ;
  // Skip whitespace.
  while (response < headers && *response == ' ') {
    ++response ;
  }

  const char * status_code_end = response ;
  while (status_code_end < headers && std::isdigit(*status_code_end)) {
    ++status_code_end ;
  }

  std::int32_t status_code = HTTP_OK ;
  if (response != status_code_end &&
      StringToInt32(
          std::string(response, status_code_end).c_str(), &status_code)) {
    SetStatusCode(status_code) ;
  } else {
    printf("WARN: Response status is omitted; assuming 200 OK\n") ;
    SetStatusCode(HTTP_OK) ;
  }

  // Parse http-headers
  std::int32_t headers_size =
      size - static_cast<std::int32_t>(headers - raw_response_) ;
  result = HttpPackageInfo::ParseInternal(headers, headers_size, false) ;
  if (V8_ERR_FAILED(result)) {
    printf("ERROR: HttpRequestInfo::Parse is failed (Line:%d)\n", __LINE__) ;
    return result ;
  }

  return result ;
}
