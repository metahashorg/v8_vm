// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/v8-http-server-session.h"

#include <set>
#include <sstream>
#include <deque>

#include "include/v8-vm.h"
#include "src/vm/utils/vm-utils.h"
#include "vm_apps/third_party/boringssl/src/include/openssl/sha.h"
#include "vm_apps/third_party/keccak-tiny/keccak-tiny.h"
#include "vm_apps/third_party/metagate/src/ethtx/rlp.h"
#include "vm_apps/third_party/metagate/src/ethtx/utils2.h"
#include "vm_apps/utils/app-utils.h"
#include "vm_apps/utils/json-sax-parser.h"
#include "vm_apps/utils/string-number-conversions.h"

namespace {

// Json field name list
const char* kJsonFieldAddress[] = JSON_ARRAY_OF_FIELD(address) ;
const char* kJsonFieldCode[] = JSON_ARRAY_OF_FIELD(code) ;
const char* kJsonFieldError[] = JSON_ARRAY_OF_FIELD(error) ;
const char* kJsonFieldFile[] = JSON_ARRAY_OF_FIELD(file) ;
const char* kJsonFieldId[] = JSON_ARRAY_OF_FIELD(id) ;
const char* kJsonFieldLine[] = JSON_ARRAY_OF_FIELD(line) ;
const char* kJsonFieldMessage[] = JSON_ARRAY_OF_FIELD(message) ;
const char* kJsonFieldResult[] = JSON_ARRAY_OF_FIELD(result) ;
const char* kJsonFieldStack[] = JSON_ARRAY_OF_FIELD(stack) ;
const char* kJsonFieldState[] = JSON_ARRAY_OF_FIELD(state) ;

const std::size_t kAddressLength = 25 ;

Error CreateAddress(
    const std::vector<std::uint8_t>& data, int nonce,
    std::string& result) {
  std::vector<std::string> fields ;
  fields.push_back(std::string(
      reinterpret_cast<const char*>(&data.at(0)), data.size())) ;
  if (nonce > 0) {
    fields.push_back(IntToRLP(nonce)) ;
  } else {
    fields.push_back("") ;
  }

  std::string rlpenc = RLP(fields) ;
  uint8_t hash[SHA256_DIGEST_LENGTH] ;
  if (sha3_256(hash, SHA256_DIGEST_LENGTH, (const uint8_t*)rlpenc.data(),
               rlpenc.size())) {
    printf("ERROR: sha3_256() is failed\n") ;
    return errUnknown ;
  }

  // Form the first 21 bytes of an address
  uint8_t address[kAddressLength] = {0} ;
  address[0] = 0x08 ;
  memcpy(address + 1, hash + 12, 20) ;
  // Calculate sha256 from the first 21 bytes of an address
  SHA256_CTX sha256_context ;
  if (!SHA256_Init(&sha256_context)) {
    printf("ERROR: SHA256_Init() is failed\n") ;
    return errUnknown ;
  }

  if (!SHA256_Update(&sha256_context, address, 21)) {
    printf("ERROR: SHA256_Update() is failed\n") ;
    return errUnknown ;
  }

  if (!SHA256_Final(hash, &sha256_context)) {
    printf("ERROR: SHA256_Final() is failed\n") ;
    return errUnknown ;
  }

  // Calculate sha256 from previous hash
  if (!SHA256_Init(&sha256_context)) {
    printf("ERROR: SHA256_Init() is failed\n") ;
    return errUnknown ;
  }

  if (!SHA256_Update(&sha256_context, hash, SHA256_DIGEST_LENGTH)) {
    printf("ERROR: SHA256_Update() is failed\n") ;
    return errUnknown ;
  }

  if (!SHA256_Final(hash, &sha256_context)) {
    printf("ERROR: SHA256_Update() is failed\n") ;
    return errUnknown ;
  }

  // Set a checksum
  address[21] = hash[0] ;
  address[22] = hash[1] ;
  address[23] = hash[2] ;
  address[24] = hash[3] ;
  // Save result
  result = "0x" + HexEncode(address, kAddressLength) ;
  return errOk ;
}

template<typename T>
Error ReadInt(
    std::uint8_t *&cur_pos, std::uint8_t *end_pos, std::uint64_t& result) {
  static_assert(std::is_integral<T>::value, "Type must be integral") ;

  // TODO: DCHECK(cur_pos + sizeof(T) <= end_pos) ;

  if ((cur_pos + sizeof(T)) > end_pos) {
    printf("ERROR: ReadInt() can't read next integer.\n") ;
    return errInvalidArgument ;
  }

  result = *reinterpret_cast<T*>(cur_pos) ;
  cur_pos += sizeof(T) ;
  return errOk ;
}

Error ReadVarInt(
    std::uint8_t *&cur_pos, std::uint8_t *end_pos, std::uint64_t& result) {
  // TODO: DCHECK(cur_pos + sizeof(std::uint8_t) <= end_pos) ;

  if ((cur_pos + sizeof(std::uint8_t)) > end_pos) {
    printf("ERROR: ReadVarInt() can't read next integer.\n") ;
    return errInvalidArgument ;
  }

  const uint8_t len = *cur_pos ;
  cur_pos++ ;
  if (len <= 249) {
    result = len ;
    return errOk ;
  } else if (len == 250) {
    return ReadInt<uint16_t>(cur_pos, end_pos, result) ;
  } else if (len == 251) {
    return ReadInt<uint32_t>(cur_pos, end_pos, result) ;
  } else if (len == 252) {
    return ReadInt<uint64_t>(cur_pos, end_pos, result) ;
  }

  printf("ERROR: Unknown type of integer.\n") ;
  return errUnsupportedType ;
}

class RequestParser {
 public:
  RequestParser() ;

  // Parse a json-request
  Error Parse(
      const char* json, const char* origin, std::int32_t size,
      std::unique_ptr<V8HttpServerSession::Request>& result) ;

 private:
   // Parse a address
   Error ParseAddress(const char* val, std::size_t size) ;

  // Parse a method
  Error ParseMethod(const std::string& method) ;

  // Parse a transaction
  Error ParseTransaction(const char* val, std::size_t size) ;

  // JSON parser callbacks
  Error OnNull() ;
  Error OnBoolean(bool val) ;
  Error OnInteger(std::int64_t val) ;
  Error OnDouble(double val) ;
  Error OnString(const char* val, std::size_t size) ;
  Error OnStartMap() ;
  Error OnMapKey(const char* val, std::size_t size) ;
  Error OnEndMap() ;
  Error OnStartArray(const char* raw_pos) ;
  Error OnEndArray(const char* raw_pos) ;

  JsonSaxParser::Callbacks callbacks_ ;
  const char* origin_ = nullptr ;
  std::unique_ptr<V8HttpServerSession::Request> request_ ;
  std::deque<std::string> nesting_ ;
  std::set<std::string> processed_ ;
  bool transaction_processing_ = false ;
  const char* params_beginning_ = nullptr ;
  std::size_t params_nesting_ = -1 ;
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
  static const char kParams[] ;
  static const char kState[] ;
  static const char kTransaction[] ;
  static const char kCompileMethod[] ;
  static const char kCmdRunMethod[] ;
  static const char* kRequestProcessedFields[] ;
  static const char* kTransactionProcessedFields[] ;
};

const char RequestParser::kStartMap[] = "{" ;
const char RequestParser::kStartArray[] = "[" ;
const char RequestParser::kAddress[] = "address" ;
const char RequestParser::kCode[] = "code" ;
const char RequestParser::kFunction[] = "function" ;
const char RequestParser::kId[] = "id" ;
const char RequestParser::kMethod[] = "method" ;
const char RequestParser::kParams[] = "params" ;
const char RequestParser::kState[] = "state" ;
const char RequestParser::kTransaction[] = "transaction" ;
const char RequestParser::kCompileMethod[] = "compile" ;
const char RequestParser::kCmdRunMethod[] = "cmdrun" ;
const char* RequestParser::kRequestProcessedFields[] = {
    kAddress, kId, kMethod, kState, kTransaction } ;
const char* RequestParser::kTransactionProcessedFields[] = {
    kFunction, kParams, kMethod, kCode } ;

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
      &RequestParser::OnStartArray, std::ref(*this), std::placeholders::_1) ;
  callbacks_.end_array_callback = std::bind(
      &RequestParser::OnEndArray, std::ref(*this), std::placeholders::_1) ;
}

Error RequestParser::Parse(
  const char* json, const char* origin, std::int32_t size,
  std::unique_ptr<V8HttpServerSession::Request>& result) {
  // Clear nesting
  nesting_.clear() ;

  // Create a new result
  std::unique_ptr<V8HttpServerSession::Request> request(
      new V8HttpServerSession::Request()) ;
  vvi::TemporarilyChangeValues<std::unique_ptr<V8HttpServerSession::Request>>
      request_remover(request_, request) ;

  // Set a processing environment
  origin_ = origin ;
  transaction_processing_ = false ;
  processed_fileds_ = kRequestProcessedFields ;
  processed_fileds_count_ = arraysize(kRequestProcessedFields) ;

  // Try to parse a json
  JsonSaxParser parser(callbacks_, JsonSaxParser::JSON_PARSE_RFC) ;
  Error res = parser.Parse(json, origin_, size) ;
  if (V8_ERROR_FAILED(res)) {
    V8_ERROR_ADD_MSG(res, "Can't have parsed the json of the request") ;
    return res ;
  }

  // Check that we have all items
  for (int i = 0; i < processed_fileds_count_; ++i) {
    if (request_->method != V8HttpServerSession::Method::CmdRun &&
        processed_fileds_[i] == kState) {
      continue ;
    }

    if (processed_.find(processed_fileds_[i]) == processed_.end()) {
      return V8_ERROR_CREATE_WITH_MSG_SP(
          errNotEnoughData, "Field |%s| is absent in the json",
          processed_fileds_[i]) ;
    }
  }

  // Save a result
  std::swap(result, request_) ;
  return errOk ;
}

Error RequestParser::ParseAddress(const char* val, std::size_t size) {
  if (size < 2 || val[0] != '0' || val[1] != 'x') {
    return V8_ERROR_CREATE_WITH_MSG(
        errInvalidArgument, "|address| has a invalid format") ;
  }

  if (!HexStringToBytes(std::string(val + 2 , size - 2), &request_->address)) {
    return V8_ERROR_CREATE_WITH_MSG(
        errInvalidArgument, "|address| has a corrupted value") ;
  }

  request_->address_str.assign(val, size) ;
  return errOk ;
}

Error RequestParser::ParseMethod(const std::string& method) {
  Error result = errOk ;
  if (method == kCompileMethod) {
    request_->method = V8HttpServerSession::Method::Compile ;
  } else if (method == kCmdRunMethod) {
    request_->method = V8HttpServerSession::Method::CmdRun ;
  } else {
    result = V8_ERROR_CREATE_WITH_MSG_SP(
        errInvalidArgument, "Unknown |method| - \'%s\'", method.c_str()) ;
  }

  return result ;
}

Error RequestParser::ParseTransaction(const char* val, std::size_t size) {
  std::vector<std::uint8_t> transaction ;
  if (!HexStringToBytes(std::string(val, size), &transaction)) {
    return V8_ERROR_CREATE_WITH_MSG(
        errInvalidArgument, "|transaction| has a corrupted value") ;
  }

  if (transaction.size() <= kAddressLength) {
    return V8_ERROR_CREATE_WITH_MSG(
        errInvalidArgument, "|transaction| is too short") ;
  }

  // We have an address from a json therefore we skip it
  std::uint8_t* position = &transaction.at(kAddressLength) ;
  std::uint8_t* end_position = &transaction.at(0) + transaction.size() ;

  // Read transaction.value
  Error result = ReadVarInt(
      position, end_position, request_->transaction.value) ;
  if (V8_ERROR_FAILED(result)) {
    V8_ERROR_ADD_MSG(result, "Can't have read |transaction.value|") ;
    return result ;
  }

  // Read transaction.fees
  result = ReadVarInt(position, end_position, request_->transaction.fees) ;
  if (V8_ERROR_FAILED(result)) {
    V8_ERROR_ADD_MSG(result, "Can't have read |transaction.fees|") ;
    return result ;
  }

  // Read transaction.nonce
  result = ReadVarInt(position, end_position, request_->transaction.nonce) ;
  if (V8_ERROR_FAILED(result)) {
    V8_ERROR_ADD_MSG(result, "Can't have read |transaction.nonce|") ;
    return result ;
  }

  // Read transaction.data_size
  std::uint64_t data_size = 0 ;
  result = ReadVarInt(position, end_position, data_size) ;
  if (V8_ERROR_FAILED(result)) {
    V8_ERROR_ADD_MSG(result, "Can't have read |transaction.data_size|") ;
    return result ;
  }

  // Read transaction.data
  if ((end_position - position) != static_cast<std::int64_t>(data_size)) {
    return V8_ERROR_CREATE_WITH_MSG(
        errInvalidArgument, "|transaction| data don't match its format") ;
  }

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
      reinterpret_cast<const char*>(position),
      (std::string(origin_) + ": |transaction.data|").c_str(),
      static_cast<std::int32_t>(data_size)) ;
  if (V8_ERROR_FAILED(result)) {
    V8_ERROR_ADD_MSG(
        result, "Can't have parsed |transaction.data| of the request") ;
    return result ;
  }

  // Check that we have all items
  for (int i = 0; i < processed_fileds_count_; ++i) {
    if (processed_.find(processed_fileds_[i]) == processed_.end()) {
      if ((request_->method != V8HttpServerSession::Method::Compile &&
           processed_fileds_[i] == kCode) ||
          processed_fileds_[i] == kFunction ||
          processed_fileds_[i] == kParams) {
        continue ;
      }

      return V8_ERROR_CREATE_WITH_MSG_SP(
          errNotEnoughData, "Field |%s| is absent in |transaction.data|",
          processed_fileds_[i]) ;
    }
  }

  // Check that we have something for executing
  if (request_->transaction.data.code.empty() &&
      request_->transaction.data.function.empty()) {
    return V8_ERROR_CREATE_WITH_MSG(
        errNotEnoughData,
        "Either |code| or |function| must be in |transaction.data|") ;
  }

  return result ;
}

Error RequestParser::OnNull() {
  if (nesting_.empty()) {
    // NOTREACHED() ;
    return V8_ERROR_CREATE_WITH_MSG(
        errJsonUnexpectedToken, "Unexpected \'null\'") ;
  }

  for (int i = 0; i < processed_fileds_count_; ++i) {
    if (nesting_.back() == processed_fileds_[i]) {
      return V8_ERROR_CREATE_WITH_MSG_SP(
          errJsonInappropriateValue, "|%s| can't be \'null\'",
          processed_fileds_[i]) ;
    }
  }

  if (nesting_.back() != kStartArray) {
    nesting_.pop_back() ;
  }

  return errOk ;
}

Error RequestParser::OnBoolean(bool val) {
  Error result = errOk ;
  if (nesting_.empty()) {
    // NOTREACHED() ;
    return V8_ERROR_CREATE_WITH_MSG(
        errJsonUnexpectedToken, "Unexpected \'boolean\'") ;
  }

  for (int i = 0; i < processed_fileds_count_; ++i) {
    if (nesting_.back() == processed_fileds_[i]) {
      return V8_ERROR_CREATE_WITH_MSG_SP(
          errJsonInappropriateType, "|%s| can't be \'boolean\'",
          processed_fileds_[i]) ;
    }
  }

  if (nesting_.back() != kStartArray) {
    nesting_.pop_back() ;
  }

  return result ;
}

Error RequestParser::OnInteger(std::int64_t val) {
  Error result = errOk ;
  if (nesting_.empty()) {
    // NOTREACHED() ;
    return V8_ERROR_CREATE_WITH_MSG(
        errJsonUnexpectedToken, "Unexpected \'integer\'") ;
  }

  if (nesting_.back() == kId) {
    request_->id = val ;
    processed_.insert(kId) ;
  } else {
    for (int i = 0; i < processed_fileds_count_; ++i) {
      if (nesting_.back() == processed_fileds_[i]) {
        return V8_ERROR_CREATE_WITH_MSG_SP(
            errJsonInappropriateType, "|%s| can't be \'integer\'",
            processed_fileds_[i]) ;
      }
    }
  }

  if (nesting_.back() != kStartArray) {
    nesting_.pop_back() ;
  }

  return result ;
}

Error RequestParser::OnDouble(double val) {
  Error result = errOk ;
  if (nesting_.empty()) {
    // NOTREACHED() ;
    return V8_ERROR_CREATE_WITH_MSG(
        errJsonUnexpectedToken, "Unexpected \'double\'") ;
  }

  for (int i = 0; i < processed_fileds_count_; ++i) {
    if (nesting_.back() == processed_fileds_[i]) {
      result = errJsonInappropriateType ;
      return V8_ERROR_CREATE_WITH_MSG_SP(
          errJsonInappropriateType, "|%s| can't be \'double\'",
          processed_fileds_[i]) ;
    }
  }

  if (nesting_.back() != kStartArray) {
    nesting_.pop_back() ;
  }

  return result ;
}

Error RequestParser::OnString(const char* val, std::size_t size) {
  Error result = errOk ;
  if (nesting_.empty()) {
    // NOTREACHED() ;
    return V8_ERROR_CREATE_WITH_MSG(
        errJsonUnexpectedToken, "Unexpected \'string\'") ;
  }

  if (nesting_.back() == kAddress) {
    result = ParseAddress(val, size) ;
    if (V8_ERROR_FAILED(result)) {
      V8_ERROR_ADD_MSG(result, "|address| parsing is failed") ;
      return result ;
    }

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
      result = ParseMethod(std::string(val, size)) ;
      if (V8_ERROR_FAILED(result)) {
        V8_ERROR_ADD_MSG(result, "|method| parsing is failed") ;
        return result ;
      }
    }

    processed_.insert(kMethod) ;
  } else if (nesting_.back() == kState) {
    if (size && !HexStringToBytes(std::string(val, size), &request_->state)) {
      return V8_ERROR_CREATE_WITH_MSG(
          errInvalidArgument, "|state| parsing is failed") ;
    }

    if (size) {
      processed_.insert(kState) ;
    }
  } else if (nesting_.back() == kTransaction) {
    result = ParseTransaction(val, size) ;
    if (V8_ERROR_FAILED(result)) {
      V8_ERROR_ADD_MSG(result, "|transaction| parsing is failed") ;
      return result ;
    }

    processed_.insert(kTransaction) ;
  } else {
    for (int i = 0; i < processed_fileds_count_; ++i) {
      if (nesting_.back() == processed_fileds_[i]) {
        return V8_ERROR_CREATE_WITH_MSG_SP(
            errJsonInappropriateType, "|%s| can't be \'string\'",
            processed_fileds_[i]) ;
      }
    }
  }

  if (nesting_.back() != kStartArray) {
    nesting_.pop_back() ;
  }

  return result ;
}

Error RequestParser::OnStartMap() {
  nesting_.emplace_back(kStartMap) ;
  return errOk ;
}

Error RequestParser::OnMapKey(const char* key, std::size_t size) {
  nesting_.emplace_back(key, size) ;
  return errOk ;
}

Error RequestParser::OnEndMap() {
  if (nesting_.empty() || nesting_.back() != kStartMap) {
    return V8_ERROR_CREATE_WITH_MSG(
        errJsonUnexpectedToken, "Unexpected the end of a map") ;
  }

  nesting_.pop_back() ;
  if (!nesting_.empty() && nesting_.back() != kStartArray) {
    nesting_.pop_back() ;
  }

  return errOk ;
}

Error RequestParser::OnStartArray(const char* raw_pos) {
  if (transaction_processing_ && !params_beginning_&& !nesting_.empty() &&
      nesting_.back() == kParams) {
    params_beginning_ = raw_pos ;
    params_nesting_ = nesting_.size() + 1 ;
  }

  nesting_.emplace_back(kStartArray) ;
  return errOk ;
}

Error RequestParser::OnEndArray(const char* raw_pos) {
  if (nesting_.empty() || nesting_.back() != kStartArray) {
    return V8_ERROR_CREATE_WITH_MSG(
        errJsonUnexpectedToken, "Unexpected the end of an array") ;
  }

  if (params_beginning_ && nesting_.size() == params_nesting_) {
    request_->transaction.data.params.assign(params_beginning_ + 1, raw_pos) ;
    params_beginning_ = nullptr ;
    processed_.insert(kParams) ;
  }

  nesting_.pop_back() ;
  if (!nesting_.empty() && nesting_.back() != kStartArray) {
    nesting_.pop_back() ;
  }

  return errOk ;
}

}  // namespace

FormattedJson V8HttpServerSession::json_formatted_ = FormattedJson::True ;

Error V8HttpServerSession::ProcessSession(
    HttpRequestInfo& request, HttpResponseInfo& response) {
  V8_LOG_FUNCTION_BODY() ;

  // We process the only post-messages
  if (request.method() != HttpRequestInfo::Method::Post) {
    response.SetStatusCode(HTTP_METHOD_NOT_ALLOWED) ;
    response.SetHeader(
        HttpPackageInfo::Header::Allow, HttpRequestInfo::Method::Post) ;
    WriteErrorResponseBody(nullptr, errNetActionNotAllowed, response) ;
    V8_LOG_RETURN errOk ;
  }

  V8HttpServerSession session(request, response) ;
  Error result = session.Do() ;
  V8_LOG_RETURN result ;
}

Error V8HttpServerSession::WriteErrorResponseBody(
    Request* request, const Error& error, HttpResponseInfo& response) {
  std::ostringstream http_body ;
  JsonGapArray gaps ;
  JsonGap root_gap(gaps, json_formatted_, 0) ;
  JsonGap child_gap(root_gap) ;
  http_body << kJsonLeftBracket[root_gap] ;

  // Write |id|
  if (request) {
    http_body << child_gap << kJsonFieldId[child_gap] << request->id
        << kJsonComma[child_gap] ;
  }

  // Write |error| field
  JsonGap error_gap(root_gap) ;
  JsonGap error_item_gap(error_gap) ;
  http_body << error_gap << kJsonFieldError[error_gap] ;
  http_body << kJsonLeftBracket[error_gap] ;
  http_body << error_item_gap << kJsonFieldCode[error_item_gap]
      << static_cast<ErrorCodeType>(error) << kJsonComma[error_item_gap] ;
  http_body << error_item_gap << kJsonFieldMessage[error_item_gap]
      << JSON_STRING(error.description()) << kJsonComma[error_item_gap] ;
  if (error.message_count()) {
    http_body << error_item_gap << kJsonFieldStack[error_item_gap]
      << kJsonLeftSquareBracket[error_item_gap] ;
    JsonGap stack_item_gap(error_item_gap) ;
    JsonGap stack_item_item_gap(stack_item_gap) ;
    for (std::size_t i = 0, size = error.message_count(); i < size; ++i) {
      if (i) {
        http_body << kJsonComma[stack_item_item_gap] ;
      }

      const Error::Message& msg = error.message(size - 1 - i) ;
      http_body << stack_item_gap << kJsonLeftBracket[stack_item_gap] ;
      http_body << stack_item_item_gap << kJsonFieldMessage[stack_item_item_gap]
          << JSON_STRING(msg.message) << kJsonComma[stack_item_item_gap] ;
      http_body << stack_item_item_gap << kJsonFieldFile[stack_item_item_gap]
          << JSON_STRING(msg.file) << kJsonComma[stack_item_item_gap] ;
      http_body << stack_item_item_gap << kJsonFieldLine[stack_item_item_gap]
          << msg.line ;
      http_body << kJsonNewLine[stack_item_gap] << stack_item_gap
          << kJsonRightBracket[stack_item_gap] ;
    }

    http_body << kJsonNewLine[error_item_gap] << error_item_gap
        << kJsonRightSquareBracket[error_item_gap] ;
  }

  http_body << kJsonNewLine[error_gap] << error_gap
      << kJsonRightBracket[error_gap] ;

  http_body << kJsonNewLine[root_gap] << kJsonRightBracket[root_gap] ;

  // Write a body
  response.SetBody(http_body.str()) ;
  return errOk ;
}

V8HttpServerSession::V8HttpServerSession(
    HttpRequestInfo& request, HttpResponseInfo& response)
  : http_request_(request),
    http_response_(response) {}

Error V8HttpServerSession::Do() {
  V8_LOG_FUNCTION_BODY() ;

  const char* body = nullptr ;
  std::int32_t body_size = 0 ;
  Error result = http_request_.GetBody(body, body_size) ;
  if (V8_ERROR_FAILED(result)) {
    V8_ERROR_ADD_MSG(result, "http_request_.GetBody(...) is failed") ;
    http_response_.SetStatusCode(HTTP_INTERNAL_SERVER_ERROR) ;
    V8_LOG_RETURN WriteErrorResponseBody(
        request_.get(), result, http_response_) ;
  }

  // TODO: Add IP-address to origin - "http-request from 127.0.0.1:8080"
  RequestParser parser ;
  result = parser.Parse(body, "http-request", body_size, request_) ;
  if (V8_ERROR_FAILED(result)) {
    http_response_.SetStatusCode(HTTP_BAD_REQUEST) ;
    V8_LOG_RETURN WriteErrorResponseBody(
        request_.get(), result, http_response_) ;
  }

  if (request_->method == Method::Compile) {
    result = CompileScript() ;
  } else if (request_->method == Method::CmdRun) {
    result = RunCommandScript() ;
  } else {
    result = V8_ERROR_CREATE_WITH_MSG(
        errJsonUnexpectedToken, "|method| is unknown") ;
    http_response_.SetStatusCode(HTTP_BAD_REQUEST) ;
    V8_LOG_RETURN WriteErrorResponseBody(
        request_.get(), result, http_response_) ;
  }

  if (V8_ERROR_FAILED(result)) {
    V8_ERROR_ADD_MSG(result, "Can't execute a js-script") ;
    http_response_.SetStatusCode(
        result == errNetInvalidPackage ? HTTP_BAD_REQUEST :
                                         HTTP_INTERNAL_SERVER_ERROR) ;
    V8_LOG_RETURN WriteErrorResponseBody(
        request_.get(), result, http_response_) ;
  }

  result = WriteResponseBody() ;
  if (V8_ERROR_FAILED(result)) {
    V8_ERROR_ADD_MSG(result, "Can't write a response body") ;
    http_response_.SetStatusCode(HTTP_INTERNAL_SERVER_ERROR) ;
    V8_LOG_RETURN WriteErrorResponseBody(
        request_.get(), result, http_response_) ;
  }

  V8_LOG_RETURN errOk ;
}

Error V8HttpServerSession::CompileScript() {
  static const char script_template[] = ";\ncontract = new %s(%s);" ;

  V8_LOG_FUNCTION_BODY() ;

  // Create script
  std::string script = request_->transaction.data.code ;
  if (request_->transaction.data.function.length()) {
    script += StringPrintf(
        script_template, request_->transaction.data.function.c_str(),
        request_->transaction.data.params.c_str()) ;
  }

  if (!script.length()) {
    V8_LOG_RETURN V8_ERROR_CREATE_WITH_MSG(
        errNetInvalidPackage, "JS-script is absent") ;
  }

  V8_LOG_VBS("Script for running: \n%s\n", script.c_str()) ;

  // Run script
  v8::StartupData data = { nullptr, 0 } ;
  Error result = vv::RunScript(
      script.c_str(), request_->address_str.c_str(), &data) ;
  if (data.raw_size) {
    response_state_ = HexEncode(data.data, data.raw_size) ;
    delete [] data.data ;
  }

  if (V8_ERROR_FAILED(result)) {
    V8_ERROR_ADD_MSG(result, "Script running is failed") ;
    V8_LOG_RETURN result ;
  }

  // Create an address of a new contract
  result = CreateAddress(
      request_->address, static_cast<int>(request_->transaction.nonce),
      response_address_) ;
  if (V8_ERROR_FAILED(result)) {
    V8_ERROR_ADD_MSG(result, "Can't create a transaction address") ;
    V8_LOG_RETURN result ;
  }

  V8_LOG_RETURN errOk ;
}

Error V8HttpServerSession::RunCommandScript() {
  static const char script_template[] = ";\ncontract.%s(%s);" ;

  V8_LOG_FUNCTION_BODY() ;

  // Create script
  std::string script = request_->transaction.data.code ;
  if (request_->transaction.data.function.length()) {
    script += StringPrintf(
        script_template, request_->transaction.data.function.c_str(),
        request_->transaction.data.params.c_str()) ;
  }

  if (!script.length()) {
    V8_LOG_RETURN V8_ERROR_CREATE_WITH_MSG(
        errNetInvalidPackage, "JS-script is absent") ;
  }

  V8_LOG_VBS("Script for running: \n%s\n", script.c_str()) ;

  // Run script
  v8::StartupData state = { reinterpret_cast<char*>(&request_->state.at(0)),
                            static_cast<int>(request_->state.size()) } ;
  v8::StartupData data = { nullptr, 0 } ;
  Error result = vv::RunScriptBySnapshot(
      state, script.c_str(), request_->address_str.c_str(),
      request_->address_str.c_str(), &data) ;
  if (data.raw_size) {
    response_state_ = HexEncode(data.data, data.raw_size) ;
    delete [] data.data ;
  }

  if (V8_ERROR_FAILED(result)) {
    V8_ERROR_ADD_MSG(result, "Script running by snapshot is failed") ;
    V8_LOG_RETURN result ;
  }

  V8_LOG_RETURN errOk ;
}

Error V8HttpServerSession::WriteResponseBody() {
  std::ostringstream http_body ;
  JsonGapArray gaps ;
  JsonGap root_gap(gaps, json_formatted_, 0) ;
  JsonGap child_gap(root_gap) ;
  http_body << kJsonLeftBracket[root_gap] ;

  // Write |id| and |result| fields
  http_body << child_gap << kJsonFieldId[child_gap] << request_->id
      << kJsonComma[child_gap] ;
  http_body << child_gap << kJsonFieldResult[child_gap] ;
  http_body << kJsonLeftBracket[root_gap] ;

  // Write |state| and |address| fields
  JsonGap result_item_gap(child_gap) ;
  http_body << result_item_gap << kJsonFieldState[result_item_gap]
      << "\"" <<response_state_ << "\"" ;
  if (response_address_.length()) {
     http_body << kJsonComma[result_item_gap] ;
     http_body << result_item_gap << kJsonFieldAddress[result_item_gap]
        << JSON_STRING(response_address_) ;
  }

  // Write the end of |result| field
  http_body << kJsonNewLine[child_gap] << child_gap
      << kJsonRightBracket[child_gap] ;

  http_body << kJsonNewLine[root_gap] << kJsonRightBracket[root_gap] ;

  // Write a body
  http_response_.SetBody(http_body.str()) ;
  return errOk ;
}
