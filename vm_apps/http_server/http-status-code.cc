// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/http-status-code.h"

const char* GetHttpReasonPhrase(HttpStatusCode code, bool with_code) {
  switch (code) {

#define HTTP_STATUS(label, code, reason) \
    case HTTP_ ## label: return (with_code ? (#code " " reason) : reason) ;
#include "vm_apps/http_server/http-status-code-list.h"
#undef HTTP_STATUS

    // TODO:
    // default:
    //   NOTREACHED() << "unknown HTTP status code " << code ;
  }

  return "" ;
}
