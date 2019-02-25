// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_WORK_CONTEXT_H_
#define V8_VM_WORK_CONTEXT_H_

#include "include/v8.h"
#include "src/base/macros.h"
#include "src/vm/utils/vm-utils.h"

namespace v8 {
namespace vm {
namespace internal {

// HandleScope can't be created by 'new()' but we need something for classes
class InitializedHandleScope {
 public:
  explicit InitializedHandleScope(Isolate* isolate)
      : handle_scope_(isolate) {}

 private:
  HandleScope handle_scope_ ;
};

// Class-helper for working with script, snapshot and so on
class WorkContext {
 public:
  enum class Type {
    Simple = 1,
    Snapshot,
  };

  // Destructor
  virtual ~WorkContext() ;

  operator Isolate* () { return isolate_ ; }

  operator Local<Context> () { return context_ ; }

  Local<Context> context() { return context_ ; }

  Isolate* isolate() { return isolate_ ; }

  Type type() { return type_ ; }

  // Creates a new WorkContext by parameters
  static WorkContext* New(
      StartupData* snapshot = nullptr, StartupData* snapshot_out = nullptr) ;

 protected:
  // Constructor
  WorkContext() ;

  // Initializes WorkContext
  virtual void Initialize(Isolate* isolate, StartupData* snapshot) ;

  // Serialize/deserialize internal fields
  static StartupData SerializeInternalFieldCallback(
      Local<Object> holder, int index, void* data) ;
  static void DeserializeInternalFieldCallback(
      Local<Object> holder, int index, StartupData payload, void* data) ;

  Type type_ = Type::Simple ;
  std::unique_ptr<Data> snapshot_data_ ;
  std::unique_ptr<StartupData> snapshot_ ;
  std::unique_ptr<ArrayBuffer::Allocator> array_buffer_allocator_ ;
  Isolate* isolate_ = nullptr ;
  std::unique_ptr<Isolate::Scope> iscope_ ;
  std::unique_ptr<InitializedHandleScope> scope_ ;
  Local<Context> context_ ;
  std::unique_ptr<Context::Scope> cscope_ ;
  bool dispose_isolate_ = true ;

  DISALLOW_COPY_AND_ASSIGN(WorkContext) ;
};

// Converts a v8::Value (e.g. v8::String) into a utf8-string
// by using WorkContext
// See src/vm/vm-utils.h for ValueToUtf8
template <class T>
static inline std::string ValueToUtf8(WorkContext& context, T value) {
  return ValueToUtf8(context.isolate(), value) ;
}

}  // namespace internal
}  // namespace vm
}  // namespace v8

#endif  // V8_VM_WORK_CONTEXT_H_
