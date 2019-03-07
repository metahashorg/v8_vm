// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/http-response-info.h"

#include <cctype>

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

HttpResponseInfo::HttpResponseInfo(const IPEndPoint* ip_endpoint)
  : HttpResponseInfo(HTTP_OK, ip_endpoint) {}

HttpResponseInfo::HttpResponseInfo(
    std::int32_t status_code, const IPEndPoint* ip_endpoint)
  : HttpPackageInfo(ip_endpoint) {
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
  raw_response_error_ = errObjNotInit ;

  HttpPackageInfo::Clear() ;
}

Error HttpResponseInfo::SetStatusCode(std::int32_t status_code) {
  if (status_code < 100 || status_code > 599) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        errInvalidArgument, "Status code is invalid - %d", status_code) ;
  }

  status_code_ = static_cast<HttpStatusCode>(status_code) ;
  return errOk ;
}

std::string HttpResponseInfo::ToString() const {
  std::string output = StringPrintf(
      "HTTP/%d.%d %d %s\r\n",
      http_version().major_value(), http_version().minor_value(),
      status_code_, GetHttpReasonPhrase(status_code_)) ;
  output += HttpPackageInfo::ToString() ;
  return output ;
}

Error HttpResponseInfo::ParseInternal(
    const char* response, std::int32_t size, bool owned) {
  raw_response_ = response ;
  raw_response_size_ = size ;
  raw_response_owned_ = owned ;
  raw_response_error_ = errOk ;

  // Find http-headers
  const char* headers = std::find(response, response + size, '\r') ;
  if ((response + size - headers) < 2 || headers[1] != '\n') {
    return V8_ERROR_CREATE_WITH_MSG(
        errInvalidArgument, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  headers += 2 ;

  // Read http-version
  Error result = ParseHttpVersion(response, headers) ;
  V8_ERROR_RETURN_IF_FAILED(result) ;

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
    V8_LOG_WRN(
        wrnArgumentOmitted, "Response status is omitted; assuming 200 OK") ;
    SetStatusCode(HTTP_OK) ;
  }

  // Parse http-headers
  std::int32_t headers_size =
      size - static_cast<std::int32_t>(headers - raw_response_) ;
  result = HttpPackageInfo::ParseInternal(headers, headers_size, false) ;
  V8_ERROR_RETURN_IF_FAILED(result) ;

  return result ;
}
