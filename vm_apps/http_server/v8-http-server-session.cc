// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/v8-http-server-session.h"

#include <set>
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

const std::size_t kAddressLength = 25 ;

vv::Error CreateAddress(
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
    return vv::errUnknown ;
  }

  // Form the first 21 bytes of an address
  uint8_t address[kAddressLength] = {0} ;
  address[0] = 0x08 ;
  memcpy(address + 1, hash + 12, 20) ;
  // Calculate sha256 from the first 21 bytes of an address
  SHA256_CTX sha256_context ;
  if (!SHA256_Init(&sha256_context)) {
    printf("ERROR: SHA256_Init() is failed\n") ;
    return vv::errUnknown ;
  }

  if (!SHA256_Update(&sha256_context, address, 21)) {
    printf("ERROR: SHA256_Update() is failed\n") ;
    return vv::errUnknown ;
  }

  if (!SHA256_Final(hash, &sha256_context)) {
    printf("ERROR: SHA256_Final() is failed\n") ;
    return vv::errUnknown ;
  }

  // Calculate sha256 from previous hash
  if (!SHA256_Init(&sha256_context)) {
    printf("ERROR: SHA256_Init() is failed\n") ;
    return vv::errUnknown ;
  }

  if (!SHA256_Update(&sha256_context, hash, SHA256_DIGEST_LENGTH)) {
    printf("ERROR: SHA256_Update() is failed\n") ;
    return vv::errUnknown ;
  }

  if (!SHA256_Final(hash, &sha256_context)) {
    printf("ERROR: SHA256_Update() is failed\n") ;
    return vv::errUnknown ;
  }

  // Set a checksum
  address[21] = hash[0] ;
  address[22] = hash[1] ;
  address[23] = hash[2] ;
  address[24] = hash[3] ;
  // Save result
  result = "0x" + HexEncode(address, kAddressLength) ;
  return vv::errOk ;
}

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
   // Parse a address
   vv::Error ParseAddress(const char* val, std::size_t size) ;

  // Parse a method
  vv::Error ParseMethod(const std::string& method) ;

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
  vv::Error OnStartArray(const char* raw_pos) ;
  vv::Error OnEndArray(const char* raw_pos) ;

  JsonSaxParser::Callbacks callbacks_ ;
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
    if (request_->method != V8HttpServerSession::Method::CmdRun &&
        processed_fileds_[i] == kState) {
      continue ;
    }

    if (processed_.find(processed_fileds_[i]) == processed_.end()) {
      printf("ERROR: Field |%s| is absent.\n", processed_fileds_[i]) ;
      return vv::errNotEnoughData ;
    }
  }

  // Save a result
  std::swap(result, request_) ;
  return vv::errOk ;
}

vv::Error RequestParser::ParseAddress(const char* val, std::size_t size) {
  if (size < 2 || val[0] != '0' || val[1] != 'x') {
    printf("ERROR: |address| has a invalid format.\n") ;
    return vv::errInvalidArgument ;
  }

  if (!HexStringToBytes(std::string(val + 2 , size - 2), &request_->address)) {
    printf("ERROR: |address| has a corrupted value.\n") ;
    return vv::errInvalidArgument ;
  }

  request_->address_str.assign(val, size) ;
  return vv::errOk ;
}

vv::Error RequestParser::ParseMethod(const std::string& method) {
  vv::Error result = vv::errOk ;
  if (method == kCompileMethod) {
    request_->method = V8HttpServerSession::Method::Compile ;
  } else if (method == kCmdRunMethod) {
    request_->method = V8HttpServerSession::Method::CmdRun ;
  } else {
    printf("ERROR: Unknown method - \'%s\'\n", method.c_str()) ;
    result = vv::errInvalidArgument ;
  }

  return result ;
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
      static_cast<std::int32_t>(data_size)) ;
  if (V8_ERR_FAILED(result)) {
    printf("ERROR: Can't have parsed a transaction data of the request\n") ;
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

  if (nesting_.back() != kStartArray) {
    nesting_.pop_back() ;
  }

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

  if (nesting_.back() != kStartArray) {
    nesting_.pop_back() ;
  }

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

  if (nesting_.back() != kStartArray) {
    nesting_.pop_back() ;
  }

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

  if (nesting_.back() != kStartArray) {
    nesting_.pop_back() ;
  }

  return vv::errOk ;
}

vv::Error RequestParser::OnString(const char* val, std::size_t size) {
  if (nesting_.empty()) {
    // NOTREACHED() ;
    printf("ERROR: Unexpected \'string\'.\n") ;
    return vv::errJsonUnexpectedToken ;
  }

  if (nesting_.back() == kAddress) {
    vv::Error result = ParseAddress(val, size) ;
    if (V8_ERR_FAILED(result)) {
      printf("ERROR: ParseAddress() is failed.\n") ;
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
      vv::Error result = ParseMethod(std::string(val, size)) ;
      if (V8_ERR_FAILED(result)) {
        printf("ERROR: ParseMethod() is failed.\n") ;
        return result ;
      }
    }

    processed_.insert(kMethod) ;
  } else if (nesting_.back() == kState) {
    if (!HexStringToBytes(std::string(val, size), &request_->state)) {
      printf("ERROR: Can't parse a previous state.\n") ;
      return vv::errInvalidArgument ;
    }

    processed_.insert(kState) ;
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

  if (nesting_.back() != kStartArray) {
    nesting_.pop_back() ;
  }

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
  if (!nesting_.empty() && nesting_.back() != kStartArray) {
    nesting_.pop_back() ;
  }

  return vv::errOk ;
}

vv::Error RequestParser::OnStartArray(const char* raw_pos) {
  if (transaction_processing_ && !params_beginning_&& !nesting_.empty() &&
      nesting_.back() == kParams) {
    params_beginning_ = raw_pos ;
    params_nesting_ = nesting_.size() + 1 ;
  }

  nesting_.emplace_back(kStartArray) ;
  return vv::errOk ;
}

vv::Error RequestParser::OnEndArray(const char* raw_pos) {
  if (nesting_.empty() || nesting_.back() != kStartArray) {
    printf("ERROR: Unexpected end of array.\n") ;
    return vv::errJsonUnexpectedToken ;
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

  if (request_->method == Method::Compile) {
    result = CompileScript() ;
  } else if (request_->method == Method::CmdRun) {
    result = RunCommandScript() ;
  } else {
    printf("ERROR: Unknown method.\n") ;
    http_response_.SetStatusCode(HTTP_BAD_REQUEST) ;
    return vv::errOk ;
  }

  if (V8_ERR_FAILED(result)) {
    printf("ERROR: Can't execute a js-script.\n") ;
    http_response_.SetStatusCode(
        result == vv::errNetInvalidPackage ? HTTP_BAD_REQUEST :
                                             HTTP_INTERNAL_SERVER_ERROR) ;
    return vv::errOk ;
  }

  result = WriteResponseBody() ;
  if (V8_ERR_FAILED(result)) {
    printf("ERROR: Can't write a response body.\n") ;
    http_response_.SetStatusCode(HTTP_INTERNAL_SERVER_ERROR) ;
    return vv::errOk ;
  }

  return vv::errOk ;
}

vv::Error V8HttpServerSession::CompileScript() {
  static const char script_template[] = ";\ncontract = new %s(%s);" ;

  printf("VERBS: V8HttpServerSession::CompileScript().\n") ;

  // Create script
  std::string script = request_->transaction.data.code ;
  if (request_->transaction.data.function.length()) {
    script += vvi::StringPrintf(
        script_template, request_->transaction.data.function.c_str(),
        request_->transaction.data.params.c_str()) ;
  }

  if (!script.length()) {
    printf("ERROR: Script is absent.\n") ;
    return vv::errNetInvalidPackage ;
  }

  printf("VERBS: Script for running: \'%s\'.\n", script.c_str()) ;

  // Run script
  v8::StartupData data = { nullptr, 0 } ;
  vv::Error result = vv::RunScript(
      script.c_str(), request_->address_str.c_str(), &data) ;
  if (data.raw_size) {
    response_state_ = HexEncode(data.data, data.raw_size) ;
    delete [] data.data ;
  }

  if (V8_ERR_FAILED(result)) {
    printf("ERROR: vv::RunScript() is failed.\n") ;
    return result ;
  }

  // Create an address of a new contract
  result = CreateAddress(
      request_->address, static_cast<int>(request_->transaction.nonce),
      response_address_) ;
  if (V8_ERR_FAILED(result)) {
    printf("ERROR: Can't create a address.\n") ;
    return result ;
  }

  return vv::errOk ;
}

vv::Error V8HttpServerSession::RunCommandScript() {
  static const char script_template[] = "contract.%s(%s);" ;

  printf("VERBS: V8HttpServerSession::RunCommandScript().\n") ;

  // Create script
  std::string script = request_->transaction.data.code ;
  if (request_->transaction.data.function.length()) {
    script += vvi::StringPrintf(
        script_template, request_->transaction.data.function.c_str(),
        request_->transaction.data.params.c_str()) ;
  }

  if (!script.length()) {
    printf("ERROR: Script is absent.\n") ;
    return vv::errNetInvalidPackage ;
  }

  printf("VERBS: Script for running: \'%s\'.\n", script.c_str()) ;

  // Run script
  v8::StartupData state = { reinterpret_cast<char*>(&request_->state.at(0)),
                            static_cast<int>(request_->state.size()) } ;
  v8::StartupData data = { nullptr, 0 } ;
  vv::Error result = vv::RunScriptBySnapshot(
      state, script.c_str(), request_->address_str.c_str(),
      request_->address_str.c_str(), &data) ;
  if (data.raw_size) {
    response_state_ = HexEncode(data.data, data.raw_size) ;
    delete [] data.data ;
  }

  if (V8_ERR_FAILED(result)) {
    printf("ERROR: vv::RunScriptBySnapshot() is failed.\n") ;
    return result ;
  }

  return vv::errOk ;
}

vv::Error V8HttpServerSession::WriteResponseBody() {
  static const char kBodyWithAddress[] =
      "{\n"
      "  \"id\": %d,\n"
      "  \"result\": {\n"
      "    \"address\": \"%s\",\n"
      "    \"state\": \"%s\"\n"
      "  }\n"
      "}" ;
  static const char kBodyWithoutAddress[] =
      "{\n"
      "  \"id\": %d,\n"
      "  \"result\": {\n"
      "    \"state\": \"%s\"\n"
      "  }\n"
      "}" ;

  if (response_address_.length()) {
    http_response_.SetBody(vvi::StringPrintf(
        kBodyWithAddress, (std::int32_t)request_->id, response_address_.c_str(),
        response_state_.c_str())) ;
  } else {
    http_response_.SetBody(vvi::StringPrintf(
        kBodyWithoutAddress, (std::int32_t)request_->id,
        response_state_.c_str())) ;
  }

  return vv::errOk ;
}
