// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_DUMPER_H_
#define V8_VM_DUMPER_H_

#include <ostream>

#include "include/v8-vm.h"

namespace v8 {
namespace vm {
namespace internal {

void CreateContextDump(
    Local<Context> context, std::ostream& result,
    FormattedJson formatted = FormattedJson::False) ;

void CreateHeapDump(Isolate* isolate, std::ostream& result) ;

void CreateHeapGraphDump(
    Isolate* isolate, std::ostream& result,
    FormattedJson formatted = FormattedJson::False) ;

}  // namespace internal
}  // namespace vm
}  // namespace v8

#endif  // V8_VM_DUMPER_H_
