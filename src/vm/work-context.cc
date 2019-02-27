// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/vm/work-context.h"

#include <map>

#include "src/api.h"
#include "src/base/format-macros.h"
#include "src/handles-inl.h"
#include "src/vm/v8-handle.h"

namespace v8 {
namespace vm {
namespace internal {

namespace {

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  ~ArrayBufferAllocator() override ;
  void* Allocate(size_t length) override ;
  void* AllocateUninitialized(size_t length) override ;
  void Free(void* data, size_t length) override ;

  // Return size of memory block
  size_t GetBlockSize(void* data) ;

 private:
  std::map<void*, std::size_t> allocation_map_ ;
};

ArrayBufferAllocator::~ArrayBufferAllocator() {
  DCHECK_EQ(allocation_map_.size(), 0) ;
  for (auto it : allocation_map_) {
    V8_LOG_ERR(
        errUnknown, "Memory leak in ArrayBuffer - pointer:0x%p length:%" PRIuS,
        it.first, it.second) ;
    free(it.first) ;
  }
}

void* ArrayBufferAllocator::Allocate(size_t length) {
#if V8_OS_AIX && _LINUX_SOURCE_COMPAT
  // Work around for GCC bug on AIX
  // See: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=79839
  void* data = __linux_calloc(length, 1) ;
#else
  void* data = calloc(length, 1) ;
#endif
  V8_LOG_VBS(
      "Allocated for ArrayBuffer - pointer:0x%p length:%" PRIuS, data, length) ;
  allocation_map_[data] = length ;
  return data ;
}

void* ArrayBufferAllocator::AllocateUninitialized(size_t length) {
#if V8_OS_AIX && _LINUX_SOURCE_COMPAT
  // Work around for GCC bug on AIX
  // See: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=79839
  void* data = __linux_malloc(length) ;
#else
  void* data = malloc(length) ;
#endif
V8_LOG_VBS(
    "Allocated for ArrayBuffer - pointer:0x%p length:%" PRIuS, data, length) ;
  allocation_map_[data] = length ;
  return data ;
}

void ArrayBufferAllocator::Free(void* data, size_t length) {
  DCHECK(allocation_map_.find(data) != allocation_map_.end()) ;
  DCHECK_EQ(allocation_map_[data], length) ;
  V8_LOG_VBS(
      "Freed for ArrayBuffer - pointer:0x%p length:%" PRIuS, data, length) ;
  allocation_map_.erase(data) ;
  free(data) ;
}

size_t ArrayBufferAllocator::GetBlockSize(void* data) {
  DCHECK(allocation_map_.find(data) != allocation_map_.end()) ;
  auto block_info = allocation_map_.find(data) ;
  if (allocation_map_.find(data) == allocation_map_.end()) {
    V8_LOG_ERR(errUnknown, "Unknown memory address: %p", data) ;
    return 0 ;
  }

  return block_info->second ;
}

class SnapshotWorkContext : public WorkContext {
 public:
  // Destructor
  ~SnapshotWorkContext() override ;

 protected:
  // Constructor
  SnapshotWorkContext(StartupData* snapshot_out) ;

  // Initializes SnapshotWorkContext
  void Initialize(Isolate* isolate, StartupData* snapshot) override ;

  StartupData* snapshot_out_ = nullptr ;
  std::unique_ptr<SnapshotCreator> snapshot_creator_ ;

  SnapshotWorkContext() = delete ;

  DISALLOW_COPY_AND_ASSIGN(SnapshotWorkContext) ;

  friend class WorkContext ;
};

SnapshotWorkContext::~SnapshotWorkContext() {
  // TODO: Fix a program crash because of corrupted/released memory
  //       or check it in new version V8
  // Reproduction - When js-script has something like that:
  // ...
  // var variable = new Object(12345n) ; // any BigIntObject
  // ...
  //
  // Couple of stacks for help:
  //
  // v8::internal::HeapObject::SizeFromMap (src/objects-inl.h:2291)
  // v8::internal::MarkCompactCollector::ProcessMarkingWorklistInternal<v8::internal::MarkCompactCollector::MarkingWorklistProcessingMode::kDefault> (src/heap/mark-compact.cc:1657)
  // v8::internal::MarkCompactCollector::MarkLiveObjects (src/heap/mark-compact.cc:1795)
  // v8::internal::MarkCompactCollector::CollectGarbage (src/heap/mark-compact.cc:461)
  // v8::internal::Heap::MarkCompact (src/heap/heap.cc:1897)
  // v8::internal::Heap::PerformGarbageCollection (src/heap/heap.cc:1746)
  // v8::internal::Heap::CollectGarbage (src/heap/heap.cc:1403)
  // v8::internal::Heap::CollectAllAvailableGarbage (src/heap/heap.cc:1251)
  // v8::SnapshotCreator::CreateBlob (src/api.cc:766)
  // v8::vm::internal::SnapshotWorkContext::~SnapshotWorkContext (src/vm/work-context.cc:86)
  //
  // v8::internal::HeapObject::SizeFromMap (src/objects-inl.h:2291)
  // v8::internal::ConcurrentMarkingVisitor::ShouldVisit (src/heap/concurrent-marking.cc:93)
  // v8::internal::ConcurrentMarking::Run (src/heap/concurrent-marking.cc:624)
  // v8::platform::WorkerThread::Run (src/libplatform/worker-thread.cc:27)

  // TODO: Fix a program crash in debug versions because of DCHECK(...)
  //       or check it in new version V8
  // Reproduction - When js-script has something like that:
  // ...
  // var variable ; // any NativeError
  // try { eval("#") ; }
  // catch(x) { variable = x ; }
  // ...
  // or
  // ...
  // var AsyncFunction = Object.getPrototypeOf(async function(){}).constructor ;
  // var variable = new AsyncFunction(...) ; // any AsyncFunction
  // ...
  // or
  // ...
  // var GeneratorFunction = Object.getPrototypeOf(function*(){}).constructor ;
  // var variable = new GeneratorFunction(...) ; // any GeneratorFunction
  // ...
  //
  // See - File:src/snapshot/partial-serializer.cc Line:83
  // DCHECK(!startup_serializer_->ReferenceMapContains(obj));

  V8_LOG_FUNCTION_BODY() ;

  // We need to close context and scopes for obtaining a snapshot
  cscope_.reset() ;
  context_.Clear() ;
  scope_.reset() ;

  // Create a snapshot
  *snapshot_out_ = snapshot_creator_->CreateBlob(
      SnapshotCreator::FunctionCodeHandling::kKeep) ;

  iscope_.reset() ;
  if (isolate_ && dispose_isolate_) {
    isolate_->Dispose() ;
  }

  snapshot_creator_.reset() ;
}

SnapshotWorkContext::SnapshotWorkContext(StartupData* snapshot_out)
  : snapshot_out_(snapshot_out) {
  type_ = Type::Snapshot ;
}

void SnapshotWorkContext::Initialize(Isolate* isolate, StartupData* snapshot) {
  V8_LOG_FUNCTION_BODY_WITH_MSG(
      snapshot ? "With a snapshot" : "Without a snapshot") ;

  DCHECK(snapshot_out_) ;

  array_buffer_allocator_.reset(new ArrayBufferAllocator()) ;
  snapshot_creator_.reset(new SnapshotCreator(
      array_buffer_allocator_.get(), nullptr, snapshot)) ;

  WorkContext::Initialize(snapshot_creator_->GetIsolate(), nullptr) ;

  snapshot_creator_->SetDefaultContext(
      context_,
      SerializeInternalFieldsCallback(&SerializeInternalFieldCallback, this)) ;
}

}  // namespace

WorkContext::~WorkContext() {
  cscope_.reset() ;
  context_.Clear() ;
  scope_.reset() ;
  iscope_.reset() ;
  if (isolate_ && dispose_isolate_) {
    isolate_->Dispose() ;
  }

  array_buffer_allocator_.reset() ;
  snapshot_.reset() ;
  snapshot_data_.reset() ;
}

WorkContext* WorkContext::New(
    StartupData* snapshot /*= nullptr*/,
    StartupData* snapshot_out /*= nullptr*/) {
  WorkContext* result = nullptr ;
  if (snapshot_out) {
    result = new SnapshotWorkContext(snapshot_out) ;
  } else {
    result = new WorkContext() ;
  }

  result->Initialize(nullptr, snapshot) ;
  return result ;
}

WorkContext::WorkContext() {}

void WorkContext::Initialize(Isolate* isolate, StartupData* snapshot) {
  V8_LOG_FUNCTION_BODY_WITH_MSG(
      snapshot ? "With a snapshot" : "Without a snapshot") ;

  if (isolate) {
    isolate_ = isolate ;
    dispose_isolate_ = false ;
  } else {
    // We need to copy snapshot if it is present
    StartupData* snapshot_pointer = nullptr ;
    if (snapshot) {
      snapshot_data_.reset(new Data(Data::Type::Snapshot)) ;
      snapshot_data_->CopyData(snapshot->data, snapshot->raw_size) ;
      snapshot_.reset(new StartupData()) ;
      snapshot_->data = snapshot_data_->data ;
      snapshot_->raw_size = snapshot_data_->size ;
      snapshot_pointer = snapshot_.get() ;
    }

    array_buffer_allocator_.reset(new ArrayBufferAllocator()) ;
    Isolate::CreateParams create_params ;
    create_params.snapshot_blob = snapshot_pointer ;
    create_params.array_buffer_allocator = array_buffer_allocator_.get() ;
    isolate_ = Isolate::New(create_params) ;
    dispose_isolate_ = true ;
  }

  iscope_.reset(new Isolate::Scope(isolate_)) ;
  scope_.reset(new InitializedHandleScope(isolate_)) ;
  context_ = Context::New(
      isolate_, nullptr, MaybeLocal<ObjectTemplate>(), MaybeLocal<Value>(),
      DeserializeInternalFieldsCallback(
          &DeserializeInternalFieldCallback, this)) ;
  cscope_.reset(new Context::Scope(context_)) ;
}

StartupData WorkContext::SerializeInternalFieldCallback(
    Local<Object> holder, int index, void* data) {
  WorkContext* context = reinterpret_cast<WorkContext*>(data) ;
  if (!context) {
    V8_LOG_ERR(errUnknown, "Context is omitted") ;
    return {nullptr, 0} ;
  }

  void* field = holder->GetAlignedPointerFromInternalField(index) ;
  StartupData result = {nullptr, 0} ;
  if (field) {
    ArrayBufferAllocator* allocator = reinterpret_cast<ArrayBufferAllocator*>(
        context->array_buffer_allocator_.get()) ;
    if (!allocator) {
      V8_LOG_ERR(errObjNotInit, "Allocator isn't initialized") ;
      return {nullptr, 0} ;
    }

    size_t field_size = allocator->GetBlockSize(field) ;
    if (field_size) {
      char* buffer = new char[field_size] ;
      result.data = buffer ;
      memcpy(buffer, field, field_size) ;
      result.raw_size = static_cast<int>(field_size) ;
    }
  }

  V8_LOG_VBS(
      "Serialized internal filed: context:%p holder:%p index:%d "
      "pointer:%p size:%d",
      data, Utils::OpenHandle(holder.operator ->()).operator ->(), index,
      field, result.raw_size) ;
  return result ;
}

void WorkContext::DeserializeInternalFieldCallback(
    Local<Object> holder, int index, StartupData payload, void* data) {
  WorkContext* context = reinterpret_cast<WorkContext*>(data) ;
  if (!context) {
    V8_LOG_ERR(errUnknown, "Context is omitted") ;
    return ;
  }

  V8_LOG_VBS(
      "Deserialized internal filed: context:%p holder:%p index:%d size:%d",
      data, Utils::OpenHandle(holder.operator ->()).operator ->(),
      index, payload.raw_size) ;

  if (payload.raw_size == 0) {
    holder->SetAlignedPointerInInternalField(index, nullptr) ;
    return ;
  }

  ArrayBufferAllocator* allocator = reinterpret_cast<ArrayBufferAllocator*>(
      context->array_buffer_allocator_.get()) ;
  if (!allocator) {
    V8_LOG_ERR(errObjNotInit, "Allocator isn't initialized") ;
    holder->SetAlignedPointerInInternalField(index, nullptr) ;
    return ;
  }

  void* field = allocator->AllocateUninitialized(payload.raw_size) ;
  memcpy(field, payload.data, payload.raw_size) ;
  holder->SetAlignedPointerInInternalField(index, field) ;
}

}  // namespace internal
}  // namespace vm
}  // namespace v8
