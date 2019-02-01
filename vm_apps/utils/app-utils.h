// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_APPS_UTILS_APP_UTILS_H_
#define V8_VM_APPS_UTILS_APP_UTILS_H_

#include <string>

#include "include/v8-vm.h"
#include "src/base/build_config.h"
#include "src/base/compiler-specific.h"
#include "src/base/macros.h"
#include "src/vm/utils/file-path.h"
#include "src/vm/utils/json-utils.h"
#include "src/vm/utils/string-printf.h"
#include "src/vm/utils/string-utils.h"
#include "vm_apps/utils/command-line.h"

// Using directives
namespace vv = v8::vm ;
namespace vvi = vv::internal ;

using v8::vm::DeinitializeLog ;
using v8::vm::DeinitializeV8 ;
using v8::vm::Error ;
using v8::vm::ErrorCodeType ;
using v8::vm::FormattedJson ;
using v8::vm::InitializeLog ;
using v8::vm::InitializeV8 ;
using v8::vm::kDefaultLogFileSize ;
using v8::vm::LogLevels ;
using v8::vm::internal::EncodeJsonString ;
using v8::vm::internal::EqualsCaseInsensitiveASCII ;
using v8::vm::internal::FilePath ;
using v8::vm::internal::GetExecutablePath ;
using v8::vm::internal::JsonGap ;
using v8::vm::internal::JsonGapArray ;
using v8::vm::internal::kJsonComma ;
using v8::vm::internal::kJsonLeftBracket ;
using v8::vm::internal::kJsonLeftSquareBracket ;
using v8::vm::internal::kJsonNewLine ;
using v8::vm::internal::kJsonRightBracket ;
using v8::vm::internal::kJsonRightSquareBracket ;
using v8::vm::internal::StringPrintf ;
using v8::vm::internal::TRIM_NONE ;
using v8::vm::internal::TRIM_LEADING ;
using v8::vm::internal::TRIM_TRAILING ;
using v8::vm::internal::TRIM_ALL ;

// Changes a file extension
std::string ChangeFileExtension(
    const char* file_name, const char* new_extension) ;

// Wrapper for to initialize V8
class V8Initializer {
 public:
  // Constructor
  V8Initializer(const CommandLine& cmd_line) ;
  // Destructor
  ~V8Initializer() ;
};

#endif  // V8_VM_APPS_UTILS_APP_UTILS_H_
