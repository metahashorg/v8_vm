// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/v8-http-server-session.h"

#include <set>
#include <deque>

#include "include/v8-vm.h"
#include "src/vm/utils/vm-utils.h"
#include "vm_apps/utils/app-utils.h"
#include "vm_apps/utils/json-sax-parser.h"
#include "vm_apps/utils/string-number-conversions.h"

namespace {

template<typename T>
vv::Error ReadInt(
    std::uint8_t *&cur_pos, std::uint8_t *end_pos, std::uint64_t& result) {
  static_assert(std::is_integral<T>::value, "Type must be integral") ;

  // TODO: DCHECK(cur_pos + sizeof(T) <= end_pos) ;

  if ((cur_pos + sizeof(T)) > end_pos) {
    printf("ERROR: ReadInt() can't read next integer.\n") ;
    return vv::errInvalidArgument ;
  }

  result = *reinterpret_cast<T*>(cur_pos) ;
  cur_pos += sizeof(T) ;
  return vv::errOk ;
}

vv::Error ReadVarInt(
    std::uint8_t *&cur_pos, std::uint8_t *end_pos, std::uint64_t& result) {
  // TODO: DCHECK(cur_pos + sizeof(std::uint8_t) <= end_pos) ;

  if ((cur_pos + sizeof(std::uint8_t)) > end_pos) {
    printf("ERROR: ReadVarInt() can't read next integer.\n") ;
    return vv::errInvalidArgument ;
  }

  const uint8_t len = *cur_pos ;
  cur_pos++ ;
  if (len <= 249) {
    result = len ;
    return vv::errOk ;
  } else if (len == 250) {
    return ReadInt<uint16_t>(cur_pos, end_pos, result) ;
  } else if (len == 251) {
    return ReadInt<uint32_t>(cur_pos, end_pos, result) ;
  } else if (len == 252) {
    return ReadInt<uint64_t>(cur_pos, end_pos, result) ;
  }

  printf("ERROR: Unknown type of integer.\n") ;
  return vv::errUnsupportedType ;
}

class RequestParser {
 public:
  RequestParser() ;

  // Parse a json-request
  vv::Error Parse(
      const char* json, std::int32_t size,
      std::unique_ptr<V8HttpServerSession::Request>& result) ;

 private:
  // Parse a transaction
  vv::Error ParseTransaction(const char* val, std::size_t size) ;

  // JSON parser callbacks
  vv::Error OnNull() ;
  vv::Error OnBoolean(bool val) ;
  vv::Error OnInteger(std::int64_t val) ;
  vv::Error OnDouble(double val) ;
  vv::Error OnString(const char* val, std::size_t size) ;
  vv::Error OnStartMap() ;
  vv::Error OnMapKey(const char* val, std::size_t size) ;
  vv::Error OnEndMap() ;
  vv::Error OnStartArray() ;
  vv::Error OnEndArray() ;

  JsonSaxParser::Callbacks callbacks_ ;
  std::unique_ptr<V8HttpServerSession::Request> request_ ;
  std::deque<std::string> nesting_ ;
  std::set<std::string> processed_ ;
  bool transaction_processing_ = false ;
  const char** processed_fileds_ = nullptr ;
  std::int32_t processed_fileds_count_ = -1 ;

  // Json constants
  static const char kStartMap[] ;
  static const char kStartArray[] ;
  static const char kAddress[] ;
  static const char kCode[] ;
  static const char kFunction[] ;
  static const char kId[] ;
  static const char kMethod[] ;
  static const char kTransaction[] ;
  static const char* kRequestProcessedFields[] ;
  static const char* kTransactionProcessedFields[] ;

  // Transaction constants
  static const std::size_t kAddressLength ;
};

const char RequestParser::kStartMap[] = "{" ;
const char RequestParser::kStartArray[] = "[" ;
const char RequestParser::kAddress[] = "address" ;
const char RequestParser::kCode[] = "code" ;
const char RequestParser::kFunction[] = "function" ;
const char RequestParser::kId[] = "id" ;
const char RequestParser::kMethod[] = "method" ;
const char RequestParser::kTransaction[] = "transaction" ;
const char* RequestParser::kRequestProcessedFields[] = {
    kAddress, kId, kMethod, kTransaction } ;
const char* RequestParser::kTransactionProcessedFields[] = {
    kFunction, kMethod, kCode } ;

const std::size_t RequestParser::kAddressLength = 25 ;

RequestParser::RequestParser() {
  callbacks_.null_callback = std::bind(
      &RequestParser::OnNull, std::ref(*this)) ;
  callbacks_.boolean_callback = std::bind(
      &RequestParser::OnBoolean, std::ref(*this), std::placeholders::_1) ;
  callbacks_.integer_callback = std::bind(
      &RequestParser::OnInteger, std::ref(*this), std::placeholders::_1) ;
  callbacks_.double_callback = std::bind(
      &RequestParser::OnDouble, std::ref(*this), std::placeholders::_1) ;
  callbacks_.string_callback = std::bind(
      &RequestParser::OnString, std::ref(*this),
      std::placeholders::_1, std::placeholders::_2) ;
  callbacks_.start_map_callback = std::bind(
      &RequestParser::OnStartMap, std::ref(*this)) ;
  callbacks_.map_key_callback = std::bind(
      &RequestParser::OnMapKey, std::ref(*this),
      std::placeholders::_1, std::placeholders::_2) ;
  callbacks_.end_map_callback = std::bind(
      &RequestParser::OnEndMap, std::ref(*this)) ;
  callbacks_.start_array_callback = std::bind(
      &RequestParser::OnStartArray, std::ref(*this)) ;
  callbacks_.end_array_callback = std::bind(
      &RequestParser::OnEndArray, std::ref(*this)) ;
}

vv::Error RequestParser::Parse(
  const char* json, std::int32_t size,
  std::unique_ptr<V8HttpServerSession::Request>& result) {
  // Clear nesting
  nesting_.clear() ;

  // Create a new result
  std::unique_ptr<V8HttpServerSession::Request> request(
      new V8HttpServerSession::Request()) ;
  vvi::TemporarilyChangeValues<std::unique_ptr<V8HttpServerSession::Request>>
      request_remover(request_, request) ;

  // Set a processing environment
  transaction_processing_ = false ;
  processed_fileds_ = kRequestProcessedFields ;
  processed_fileds_count_ = arraysize(kRequestProcessedFields) ;

  // Try to parse a json
  JsonSaxParser parser(callbacks_, JsonSaxParser::JSON_PARSE_RFC) ;
  vv::Error res = parser.Parse(json, size) ;
  if (V8_ERR_FAILED(res)) {
    printf("ERROR: Can't have parsed a json of the request\n") ;
    return res ;
  }

  // Check that we have all items
  for (int i = 0; i < processed_fileds_count_; ++i) {
    if (processed_.find(processed_fileds_[i]) == processed_.end()) {
      printf("ERROR: Field |%s| is absent.\n", processed_fileds_[i]) ;
      return vv::errNotEnoughData ;
    }
  }

  // Save a result
  std::swap(result, request_) ;
  return vv::errOk ;
}

vv::Error RequestParser::ParseTransaction(const char* val, std::size_t size) {
  std::vector<std::uint8_t> transaction ;
  if (!HexStringToBytes(std::string(val, size), &transaction)) {
    printf("ERROR: |transaction| has a corrupted value.\n") ;
    return vv::errInvalidArgument ;
  }

  if (transaction.size() <= kAddressLength) {
    printf("ERROR: |transaction| is too short.\n") ;
    return vv::errInvalidArgument ;
  }

  // We have an address from a json therefore we skip it
  std::uint8_t* position = &transaction.at(kAddressLength) ;
  std::uint8_t* end_position = &transaction.at(0) + transaction.size() ;

  // Read transaction.value
  vv::Error result = ReadVarInt(
      position, end_position, request_->transaction.value) ;
  if (V8_ERR_FAILED(result)) {
    printf("ERROR: Can't have read |transaction| value.\n") ;
    return result ;
  }

  // Read transaction.fees
  result = ReadVarInt(position, end_position, request_->transaction.fees) ;
  if (V8_ERR_FAILED(result)) {
    printf("ERROR: Can't have read |transaction| fees.\n") ;
    return result ;
  }

  // Read transaction.nonce
  result = ReadVarInt(position, end_position, request_->transaction.nonce) ;
  if (V8_ERR_FAILED(result)) {
    printf("ERROR: Can't have read |transaction| nonce.\n") ;
    return result ;
  }

  // Read transaction.data_size
  std::uint64_t data_size = 0 ;
  result = ReadVarInt(position, end_position, data_size) ;
  if (V8_ERR_FAILED(result)) {
    printf("ERROR: Can't have read |transaction| data size.\n") ;
    return result ;
  }

  // Read transaction.data
  if ((end_position - position) != static_cast<std::int64_t>(data_size)) {
    printf("ERROR: |transaction| data don't match its format.\n") ;
    return vv::errInvalidArgument ;
  }

  request_->transaction.data_str.assign(
      reinterpret_cast<const char*>(position), data_size) ;

  // Parse transaction.data
  vvi::TemporarilySetValue<std::deque<std::string>> nesting(
      nesting_, std::deque<std::string>()) ;
  vvi::TemporarilySetValue<std::set<std::string>> processed(
      processed_, std::set<std::string>()) ;
  vvi::TemporarilySetValue<bool> transaction_processing(
      transaction_processing_, true) ;
  vvi::TemporarilySetValue<const char**> processed_fileds(
      processed_fileds_, kTransactionProcessedFields) ;
  vvi::TemporarilySetValue<std::int32_t> processed_fileds_count(
      processed_fileds_count_, arraysize(kTransactionProcessedFields)) ;
      JsonSaxParser parser(callbacks_, JsonSaxParser::JSON_PARSE_RFC) ;
  result = parser.Parse(
      request_->transaction.data_str.c_str(),
      static_cast<std::int32_t>(request_->transaction.data_str.length())) ;
  if (V8_ERR_FAILED(result)) {
    printf("ERROR: Can't have parsed a transaction data of the request\n") ;
    return result ;
  }

  // Check that we have all items
  for (int i = 0; i < processed_fileds_count_; ++i) {
    if (processed_.find(processed_fileds_[i]) == processed_.end()) {
      printf("ERROR: Field |%s| of a transaction data is absent.\n",
             processed_fileds_[i]) ;
      return vv::errNotEnoughData ;
    }
  }

  return vv::errOk ;
}

vv::Error RequestParser::OnNull() {
  if (nesting_.empty()) {
    // NOTREACHED() ;
    printf("ERROR: Unexpected \'null\'.\n") ;
    return vv::errJsonUnexpectedToken ;
  }

  for (int i = 0; i < processed_fileds_count_; ++i) {
    if (nesting_.back() == processed_fileds_[i]) {
      printf("ERROR: |%s| can't be \'null\'.\n", processed_fileds_[i]) ;
      return vv::errJsonInappropriateType ;
    }
  }

  nesting_.pop_back() ;
  return vv::errOk ;
}

vv::Error RequestParser::OnBoolean(bool val) {
  if (nesting_.empty()) {
    // NOTREACHED() ;
    printf("ERROR: Unexpected \'boolean\'.\n") ;
    return vv::errJsonUnexpectedToken ;
  }

  for (int i = 0; i < processed_fileds_count_; ++i) {
    if (nesting_.back() == processed_fileds_[i]) {
      printf("ERROR: |%s| can't be \'boolean\'.\n", processed_fileds_[i]) ;
      return vv::errJsonInappropriateType ;
    }
  }

  nesting_.pop_back() ;
  return vv::errOk ;
}

vv::Error RequestParser::OnInteger(std::int64_t val) {
  if (nesting_.empty()) {
    // NOTREACHED() ;
    printf("ERROR: Unexpected \'integer\'.\n") ;
    return vv::errJsonUnexpectedToken ;
  }

  if (nesting_.back() == kId) {
    request_->id = val ;
    processed_.insert(kId) ;
  } else {
    for (int i = 0; i < processed_fileds_count_; ++i) {
      if (nesting_.back() == processed_fileds_[i]) {
        printf("ERROR: |%s| can't be \'integer\'.\n", processed_fileds_[i]) ;
        return vv::errJsonInappropriateType ;
      }
    }
  }

  nesting_.pop_back() ;
  return vv::errOk ;
}

vv::Error RequestParser::OnDouble(double val) {
  if (nesting_.empty()) {
    // NOTREACHED() ;
    printf("ERROR: Unexpected \'double\'.\n") ;
    return vv::errJsonUnexpectedToken ;
  }

  for (int i = 0; i < processed_fileds_count_; ++i) {
    if (nesting_.back() == processed_fileds_[i]) {
      printf("ERROR: |%s| can't be \'double\'.\n", processed_fileds_[i]) ;
      return vv::errJsonInappropriateType ;
    }
  }

  nesting_.pop_back() ;
  return vv::errOk ;
}

vv::Error RequestParser::OnString(const char* val, std::size_t size) {
  if (nesting_.empty()) {
    // NOTREACHED() ;
    printf("ERROR: Unexpected \'string\'.\n") ;
    return vv::errJsonUnexpectedToken ;
  }

  if (nesting_.back() == kAddress) {
    request_->address.assign(val, size) ;
    processed_.insert(kAddress) ;
  } else if (nesting_.back() == kCode) {
    request_->transaction.data.code.assign(val, size) ;
    processed_.insert(kCode) ;
  } else if (nesting_.back() == kFunction) {
    request_->transaction.data.function.assign(val, size) ;
    processed_.insert(kFunction) ;
  } else if (nesting_.back() == kMethod) {
    if (transaction_processing_) {
      request_->transaction.data.method.assign(val, size) ;
    } else {
      request_->method.assign(val, size) ;
    }

    processed_.insert(kMethod) ;
  } else if (nesting_.back() == kTransaction) {
    vv::Error result = ParseTransaction(val, size) ;
    if (V8_ERR_FAILED(result)) {
      printf("ERROR: ParseTransaction() is failed.\n") ;
      return result ;
    }

    processed_.insert(kTransaction) ;
  } else {
    for (int i = 0; i < processed_fileds_count_; ++i) {
      if (nesting_.back() == processed_fileds_[i]) {
        printf("ERROR: |%s| can't be \'string\'.\n", processed_fileds_[i]) ;
        return vv::errJsonInappropriateType ;
      }
    }
  }

  nesting_.pop_back() ;
  return vv::errOk ;
}

vv::Error RequestParser::OnStartMap() {
  nesting_.emplace_back(kStartMap) ;
  return vv::errOk ;
}

vv::Error RequestParser::OnMapKey(const char* key, std::size_t size) {
  nesting_.emplace_back(key, size) ;
  return vv::errOk ;
}

vv::Error RequestParser::OnEndMap() {
  if (nesting_.empty() || nesting_.back() != kStartMap) {
    printf("ERROR: Unexpected end of map.\n") ;
    return vv::errJsonUnexpectedToken ;
  }

  nesting_.pop_back() ;
  if (!nesting_.empty()) {
    nesting_.pop_back() ;
  }

  return vv::errOk ;
}

vv::Error RequestParser::OnStartArray() {
  nesting_.emplace_back(kStartArray) ;
  return vv::errOk ;
}

vv::Error RequestParser::OnEndArray() {
  if (nesting_.empty() || nesting_.back() != kStartArray) {
    printf("ERROR: Unexpected end of array.\n") ;
    return vv::errJsonUnexpectedToken ;
  }

  nesting_.pop_back() ;
  if (!nesting_.empty()) {
    nesting_.pop_back() ;
  }

  return vv::errOk ;
}

}  // namespace

vv::Error V8HttpServerSession::ProcessSession(
    HttpRequestInfo& request, HttpResponseInfo& response) {
  printf("VERBS: V8HttpServerSession::ProcessSession()\n") ;

  // We process the only post-messages
  if (request.method() != HttpRequestInfo::Method::Post) {
    response.SetStatusCode(HTTP_METHOD_NOT_ALLOWED) ;
    response.SetHeader(
        HttpPackageInfo::Header::Allow, HttpRequestInfo::Method::Post) ;
    return vv::errOk ;
  }

  V8HttpServerSession session(request, response) ;
  vv::Error result = session.Do() ;
  return result ;
}

V8HttpServerSession::V8HttpServerSession(
    HttpRequestInfo& request, HttpResponseInfo& response)
  : http_request_(request),
    http_response_(response) {}

vv::Error V8HttpServerSession::Do() {
  printf("VERBS: V8HttpServerSession::Do()\n") ;

  const char* body = nullptr ;
  std::int32_t body_size = 0 ;
  vv::Error result = http_request_.GetBody(body, body_size) ;
  if (V8_ERR_FAILED(result)) {
    printf("ERROR: http_request_.GetBody() is failed\n") ;
    http_response_.SetStatusCode(HTTP_INTERNAL_SERVER_ERROR) ;
    return vv::errOk ;
  }

  RequestParser parser ;
  result = parser.Parse(body, body_size, request_) ;
  if (V8_ERR_FAILED(result)) {
    printf("ERROR: parser.Parse() is failed\n") ;
    http_response_.SetStatusCode(HTTP_BAD_REQUEST) ;
    return vv::errOk ;
  }

  return vv::errOk ;
}
