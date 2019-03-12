// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/http-server-session.h"

namespace {

const std::int32_t kHeaderBufferSize = 4 * 1024 ;
const std::size_t kHeaderMaxSize = 4 * 1024 * 1024 ;

}  // namespace

HttpServerSession::~HttpServerSession() {}

TcpServerSession::Creator HttpServerSession::GetCreator(
    const SessionHandler& session_handler, const ErrorHandler& error_handler,
    const std::string& server_name, std::int32_t body_buffer_size) {
  return std::bind(
      &HttpServerSession::New, std::placeholders::_1, session_handler,
      error_handler, server_name, body_buffer_size) ;
}

HttpServerSession::HttpServerSession(
    std::unique_ptr<StreamSocket>& socket,
    const SessionHandler& session_handler, const ErrorHandler& error_handler,
    const std::string& server_name, std::int32_t body_buffer_size)
  : TcpServerSession(socket),
    server_name_(server_name),
    body_buffer_size_(body_buffer_size),
    session_handler_(session_handler),
    error_handler_(error_handler) {}

Error HttpServerSession::GetBody(
    const char*& body, std::int32_t& body_size, bool& owned) {
  DCHECK(request_) ;

  V8_LOG_FUNCTION_BODY() ;

  if (!request_) {
    V8_LOG_RETURN V8_ERROR_CREATE_WITH_MSG(
        errObjNotInit, "Can't get a body when request is NULL") ;
  }

  // |owned| is false in any case
  owned = false ;

  std::int32_t local_body_size = request_->content_length() ;
  if (local_body_size <= 0) {
    // TODO: Sometimes we need to read before EOF
    body = nullptr ;
    body_size = 0 ;
    V8_LOG_RETURN errOk ;
  }

  // We might have already read something
  local_body_size -= raw_body_.size() ;

  Error result = errOk ;
  std::vector<char> body_buffer(body_buffer_size_) ;
  for (; V8_ERROR_SUCCESS(result) && local_body_size > 0;) {
    std::int32_t buf_len = std::min(
        local_body_size, static_cast<std::int32_t>(body_buffer.size())) ;
    result = Read(&body_buffer.at(0), buf_len, true) ;
    if (buf_len == 0 || V8_ERROR_FAILED(result)) {
      continue ;
    }

    // Save body
    raw_body_.insert(
        raw_body_.end(), body_buffer.begin(),
        body_buffer.begin() + buf_len) ;
    local_body_size -= buf_len ;
  }

  body = &raw_body_.at(0) ;
  body_size = static_cast<std::int32_t>(raw_body_.size()) ;
  V8_LOG_RETURN result ;
}

Error HttpServerSession::ReadRequestHeader() {
  // Read http-headers
  Error result = errOk ;
  IPEndPoint local_address(GetLocalAddress()) ;
  std::vector<char> headers_buffer(kHeaderBufferSize) ;
  std::uint8_t http_header_end_flag = 0 ;
  for (; V8_ERROR_SUCCESS(result);) {
    std::int32_t buf_len = static_cast<std::int32_t>(headers_buffer.size()) ;
    result = Read(&headers_buffer.at(0), buf_len, false) ;
    if (buf_len == 0 || V8_ERROR_FAILED(result)) {
      continue ;
    }

    // Try to find the end of http-headers
    std::int32_t http_header_size = 0 ;
    for (int i = 0; i < buf_len && http_header_end_flag != 4; ++i) {
      ++http_header_size ;
      if (headers_buffer[i] == '\r' &&
          (http_header_end_flag == 0 || http_header_end_flag == 2)) {
        ++http_header_end_flag ;
        continue ;
      }

      if (headers_buffer[i] == '\n' &&
          (http_header_end_flag == 1 || http_header_end_flag == 3)) {
        ++http_header_end_flag ;
        continue ;
      }

      http_header_end_flag = 0 ;
    }

    // Save http-headers
    raw_headers_.insert(
        raw_headers_.end(), headers_buffer.begin(),
        headers_buffer.begin() + http_header_size) ;

    // Save a body if we've read part of it
    if (http_header_size < buf_len) {
      raw_body_.insert(
          raw_body_.end(), headers_buffer.begin() + http_header_size,
          headers_buffer.begin() + buf_len) ;
    }

    if (http_header_end_flag == 4) {
      break ;
    }

    if (raw_headers_.size() > kHeaderMaxSize) {
      result = errNetEntityTooLarge ;
      response_.reset(new HttpResponseInfo(
          HTTP_REQUEST_ENTITY_TOO_LARGE, &local_address)) ;
      if (error_handler_) {
        error_handler_(result, *response_) ;
      }

      break ;
    }
  }

  V8_ERROR_RETURN_IF_FAILED(result) ;

  // Parse http-headers
  IPEndPoint peer_address(GetPeerAddress()) ;
  request_.reset(new HttpRequestInfo(&peer_address)) ;
  result = request_->Parse(
      &raw_headers_.at(0), static_cast<std::int32_t>(raw_headers_.size())) ;
  if (V8_ERROR_FAILED(result)) {
    request_.reset() ;
    response_.reset(new HttpResponseInfo(HTTP_BAD_REQUEST, &local_address)) ;
    if (error_handler_) {
      error_handler_(result, *response_) ;
    }

    return V8_ERROR_CREATE_BASED_ON_WITH_MSG(
        errNetInvalidPackage, result, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  response_.reset(new HttpResponseInfo(&local_address)) ;
  request_->SetBody(std::bind(
      &HttpServerSession::GetBody, std::ref(*this), std::placeholders::_1,
      std::placeholders::_2, std::placeholders::_3)) ;
  return errOk ;
}

Error HttpServerSession::SendResponse() {
  if (!response_) {
    return wrnObjNotInit ;
  }

  // Send headers
  std::string headers = response_->ToString() ;
  std::int32_t buf_len = static_cast<std::int32_t>(headers.length()) ;
  Error result = Write(headers.c_str(), buf_len) ;
  V8_ERROR_RETURN_IF_FAILED(result) ;

  // Send a body if it exists
  const char* body = nullptr ;
  result = response_->GetBody(body, buf_len) ;
  V8_ERROR_RETURN_IF_FAILED(result) ;

  if (buf_len > 0) {
    result = Write(body, buf_len) ;
    V8_ERROR_RETURN_IF_FAILED(result) ;
  }

  return result ;
}

void HttpServerSession::SetResponseDefaultHeaders() {
  if (!response_) {
    return ;
  }

  if (!server_name_.empty()) {
    response_->SetHeaderIfMissing(
        HttpResponseInfo::Header::Server, server_name_) ;
  }
}

Error HttpServerSession::Do() {
  V8_LOG_FUNCTION_BODY() ;

  // Read a request
  Error result = ReadRequestHeader() ;
  SetResponseDefaultHeaders() ;
  if (V8_ERROR_FAILED(result)) {
    V8_ERROR_ADD_MSG(result, "ReadRequestHeader(...) is failed") ;
    SendResponse() ;
    V8_LOG_RETURN result ;
  }

  V8_LOG_INF(
      "HTTP-session - IP:\'%s\' Method:\'%s\' Host: \'%s\' Uri:\'%s\'",
      GetPeerAddress().ToString().c_str(), request_->method().c_str(),
      request_->host().c_str(), request_->uri().c_str()) ;

  // Call a handler if it exists
  IPEndPoint local_address(GetLocalAddress()) ;
  if (session_handler_) {
    result = session_handler_(*request_, *response_) ;
    if (V8_ERROR_FAILED(result)) {
      V8_ERROR_ADD_MSG(result, "session_handler_(...) is failed") ;
      // Refresh a response because of a handler call
      response_.reset(new HttpResponseInfo(
          HTTP_INTERNAL_SERVER_ERROR, &local_address)) ;
      SetResponseDefaultHeaders() ;
      if (error_handler_) {
        error_handler_(result, *response_) ;
      }
    }
  } else {
    response_->SetStatusCode(HTTP_NOT_IMPLEMENTED) ;
    SetResponseDefaultHeaders() ;
    if (error_handler_) {
      error_handler_(errNotImplemented, *response_) ;
    }
  }

  // Send a response
  result = SendResponse() ;
  V8_LOG_RETURN result ;
}

TcpServerSession* HttpServerSession::New(
    std::unique_ptr<StreamSocket>& socket,
    const SessionHandler& session_handler, const ErrorHandler& error_handler,
    const std::string& server_name, std::int32_t body_buffer_size) {
  if (!socket || body_buffer_size < 1) {
    return nullptr ;
  }

  return new HttpServerSession(
      socket, session_handler, error_handler, server_name, body_buffer_size) ;
}
