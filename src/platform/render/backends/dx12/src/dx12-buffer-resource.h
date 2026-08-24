#pragma once

#ifdef ENGINE_RENDERER_DX12

#include <D3D12MemAlloc.h>
#include <d3d12.h>

namespace eng::render {

/// Internal D3D12 resource data for a buffer handle.
struct Dx12Buffer {
  /// D3D12 committed/placed resource.
  ID3D12Resource* resource = nullptr;
  /// D3D12MA allocation backing this buffer.
  D3D12MA::Allocation* allocation = nullptr;
  /// Whether this buffer is CPU-mappable (upload heap).
  bool host_visible = false;
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_DX12
