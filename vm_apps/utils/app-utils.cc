// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/utils/app-utils.h"

#include <cstdio>
#include <cwchar>
#include <locale>

#include "src/base/macros.h"

std::string ChangeFileExtension(
    const char* file_name, const char* new_extension) {
  FilePath file_path(file_name) ;
  return file_path.ReplaceExtension(new_extension).value() ;
}
