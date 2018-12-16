// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_HTTP_SERVER_HTTP_PACKAGE_INFO_H_
#define V8_VM_APPS_HTTP_SERVER_HTTP_PACKAGE_INFO_H_

#include <functional>
#include <string>
#include <vector>

#include "src/base/macros.h"
#include "vm_apps/http_server/http-version.h"
#include "vm_apps/utils/app-utils.h"

class HttpPackageInfo {
 public:
  struct HeaderKeyValuePair {
    HeaderKeyValuePair() ;
    HeaderKeyValuePair(
        const std::string& key, const std::string& value) ;

    std::string key ;
    std::string value ;
  };

  typedef std::vector<HeaderKeyValuePair> HeaderVector ;

  struct Header {
    // General header names.
    // https://tools.ietf.org/html/rfc2616#section-4.5
    // Field names are case-insensitive.
    // https://tools.ietf.org/html/rfc2616#section-4.2
    static const char CacheControl[] ;
    static const char Connection[] ;
    static const char Date[] ;
    static const char Pragma[] ;
    static const char Trailer[] ;
    static const char TransferEncoding[] ;
    static const char Upgrade[] ;
    static const char Via[] ;
    static const char Warning[] ;

    // Entity header names.
    // https://tools.ietf.org/html/rfc2616#section-7.1
    // Field names are case-insensitive.
    // https://tools.ietf.org/html/rfc2616#section-4.2
    static const char Allow[] ;
    static const char ContentEncoding[] ;
    static const char ContentLanguage[] ;
    static const char ContentLength[] ;
    static const char ContentLocation[] ;
    static const char ContentMD5[] ;
    static const char ContentRange[] ;
    static const char ContentType[] ;
    static const char Expires[] ;
    static const char LastModified[] ;
  };

  HttpPackageInfo() ;
  virtual ~HttpPackageInfo() ;

  // Clears the object
  virtual void Clear() ;

  // Initializes the object by raw data
  vv::Error Parse(const char* data, std::int32_t size, bool owned = false) ;

  // Initializes a http-version of the object by raw-data
  vv::Error ParseHttpVersion(const char* str_begin, const char* str_end) ;

  // Sets http-version
  void SetHttpVersion(const HttpVersion& http_version) ;

  // Gets the first header that matches |key|.  If found, returns true and
  // writes the value to |out|.
  bool GetHeader(const std::string& key, std::string& out) const ;

  bool HasHeader(const std::string& key) const {
    return FindHeader(key) != headers_.end() ;
  }

  // Removes the first header that matches (case-insensitive) |key|.
  void RemoveHeader(const std::string& key) ;

  // Sets the header value pair for |key| and |value|.  If |key| already exists,
  // then the header value is modified, but the key is untouched, and the order
  // in the vector remains the same.  When comparing |key|, case is ignored.
  // The caller must ensure that |key| passes IsValidHeaderName() and
  // |value| passes IsValidHeaderValue().
  vv::Error SetHeader(const std::string& key, const std::string& value) ;

  // Sets the header value pair for |key| and |value|, if |key| does not exist.
  // If |key| already exists, the call is a no-op.
  // When comparing |key|, case is ignored.
  vv::Error SetHeaderIfMissing(
      const std::string& key, const std::string& value) ;

  // Returns body information
  vv::Error GetBody(const char*& body, std::int32_t& body_size) ;

  // Sets body information
  void SetBody(const char* body, std::int32_t body_size, bool owned = false) ;

  // Set body information by first request of it
  typedef std::function<vv::Error(const char*&, std::int32_t&, bool& owned)>
      BodyGetter ;
  void SetBody(BodyGetter getter) ;

  // Serializes HttpPackageInfo to a string representation.  Joins all the
  // header keys and values with ": ", and inserts "\r\n" between each header
  // line, and adds the trailing "\r\n". Body won't be included.
  virtual std::string ToString() const ;

  std::int32_t content_length() const { return content_length_ ; }

  HttpVersion http_version() const { return http_version_ ; }

  vv::Error raw_headers(const char*& headers, std::int32_t& size) const {
    headers = raw_headers_ ;
    size = raw_headers_size_ ;
    return raw_headers_error_ ;
  }

  // See RFC 7230 Sec 3.2.6 for the definition of |token|.
  static bool IsToken(const std::string& string) ;

 protected:
  virtual vv::Error ParseImpl(
    const char* headers, std::int32_t size, bool owned) ;

  virtual void UpdateInfoByHeader(
      const std::string& key, const std::string& value, bool deleted = false) ;

 private:
  HeaderVector::iterator FindHeader(const std::string& key) ;
  HeaderVector::const_iterator FindHeader(const std::string& key) const ;

  HttpVersion http_version_ = HttpVersion(1, 1) ;

  HeaderVector headers_ ;

  // Headers data
  const char* raw_headers_ = nullptr ;
  std::int32_t raw_headers_size_ = 0 ;
  bool raw_headers_owned_ = false ;
  vv::Error raw_headers_error_ = vv::errObjNotInit ;

  // Body data
  const char* body_ = nullptr ;
  std::int32_t content_length_ = -1 ;
  std::int32_t body_size_ = 0 ;
  bool body_owned_ = false ;
  vv::Error body_error_ = vv::wrnObjNotInit ;
  BodyGetter body_getter_ = nullptr ;

  DISALLOW_COPY_AND_ASSIGN(HttpPackageInfo) ;
};

#endif  // V8_VM_APPS_HTTP_SERVER_HTTP_PACKAGE_INFO_H_
