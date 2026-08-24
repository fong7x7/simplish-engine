#pragma once

#ifdef ENGINE_RENDERER_DX12

#include <cstdint>
#include <d3d12.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace eng::render {

/// Per-frame synchronization and command recording state for DX12.
struct Dx12PerFrameData {
  /// Command allocator for this frame's command lists.
  ID3D12CommandAllocator* command_allocator = nullptr;
  /// Primary graphics command list for this frame.
  ID3D12GraphicsCommandList* command_list = nullptr;
  /// Fence for tracking GPU completion of this frame.
  ID3D12Fence* fence = nullptr;
  /// Monotonically increasing fence value for this frame.
  uint64_t fence_value = 0;
#ifdef _WIN32
  /// Win32 event handle for CPU-side fence wait.
  HANDLE fence_event = nullptr;
#endif
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_DX12
