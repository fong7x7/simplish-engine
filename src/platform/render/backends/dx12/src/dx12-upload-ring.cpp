#include "dx12-upload-ring.h"

#ifdef ENGINE_RENDERER_DX12

#include <cstring>

namespace eng::render {

namespace {

  /// A root constant buffer view has to start on a 256-byte boundary.
  constexpr uint64_t DX12_CBV_ALIGNMENT = 256;

  uint64_t alignUp(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
  }

  D3D12_RESOURCE_DESC buildUploadBufferDesc(uint64_t capacity) {
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = capacity;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return desc;
  }

}  // namespace

bool Dx12UploadRing::create(D3D12MA::Allocator* allocator, uint64_t capacity) {
  D3D12MA::ALLOCATION_DESC alloc_desc{};
  alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
  auto desc = buildUploadBufferDesc(capacity);
  HRESULT hr = allocator->CreateResource(
      &alloc_desc, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      &allocation_, IID_PPV_ARGS(&resource_));
  void* mapped = nullptr;
  if (FAILED(hr) || FAILED(resource_->Map(0, nullptr, &mapped))) {
    return false;
  }
  mapped_ = static_cast<uint8_t*>(mapped);
  base_ = resource_->GetGPUVirtualAddress();
  capacity_ = capacity;
  return true;
}

void Dx12UploadRing::destroy() {
  if (resource_ != nullptr) {
    resource_->Unmap(0, nullptr);
    resource_->Release();
    resource_ = nullptr;
  }
  if (allocation_ != nullptr) {
    allocation_->Release();
    allocation_ = nullptr;
  }
  mapped_ = nullptr;
  capacity_ = 0;
  cursor_ = 0;
}

void Dx12UploadRing::reset() {
  cursor_ = 0;
}

D3D12_GPU_VIRTUAL_ADDRESS Dx12UploadRing::push(const void* data,
                                               uint64_t size) {
  const uint64_t offset = alignUp(cursor_, DX12_CBV_ALIGNMENT);
  if (mapped_ == nullptr || data == nullptr || size == 0) {
    return 0;
  }
  if (offset + size > capacity_) {
    return 0;
  }
  std::memcpy(mapped_ + offset, data, size);
  cursor_ = offset + size;
  return base_ + offset;
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_DX12
