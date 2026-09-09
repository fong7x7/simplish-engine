#pragma once

#ifdef ENGINE_RENDERER_DX12

/// @file dx12-upload-ring.h
/// @brief Per-frame linear upload buffer backing root constant buffer views.
/// @par Threading Main-thread-only, like the rest of the DX12 backend.

#include <D3D12MemAlloc.h>
#include <cstdint>
#include <d3d12.h>

namespace eng::render {

/// One frame's worth of small constants written by the CPU.
///
/// `setVertexStageBytes` and `setFragmentStageBytes` hand the backend a few
/// hundred bytes that have to stay put until the frame's commands retire.
/// Root constants would be the cheap answer, but the mesh light block alone
/// is 100 DWORDs and a whole root signature holds 64 — so the bytes land
/// here and the command list binds a root CBV at the address returned.
///
/// The buffer is reset, not freed, at the top of each frame, which is safe
/// because `beginFrame` has already waited on that frame's fence.
class Dx12UploadRing {
public:
  /// Allocate the backing upload buffer. False if the allocation fails.
  bool create(D3D12MA::Allocator* allocator, uint64_t capacity);

  /// Release the buffer and its allocation.
  void destroy();

  /// Hand back every byte allocated since the last reset.
  void reset();

  /// Copy `size` bytes in and return the GPU address to bind them at, or 0
  /// when this frame's budget is spent.
  D3D12_GPU_VIRTUAL_ADDRESS push(const void* data, uint64_t size);

private:
  /// Upload-heap resource holding this frame's constants.
  ID3D12Resource* resource_ = nullptr;
  /// D3D12MA allocation backing `resource_`.
  D3D12MA::Allocation* allocation_ = nullptr;
  /// Persistent CPU mapping of `resource_`.
  uint8_t* mapped_ = nullptr;
  /// GPU address of byte zero of `resource_`.
  D3D12_GPU_VIRTUAL_ADDRESS base_ = 0;
  /// Bytes handed out so far this frame.
  uint64_t cursor_ = 0;
  /// Total bytes in `resource_`.
  uint64_t capacity_ = 0;
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_DX12
