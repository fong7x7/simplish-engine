#pragma once

#ifdef ENGINE_RENDERER_DX12

#include "dx12-descriptor-heap-allocator.h"

#include <D3D12MemAlloc.h>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <engine/render/rhi-core-types.h>
#include <optional>

namespace eng::render {

/// Internal D3D12 resource data for a texture handle.
struct Dx12Texture {
  /// D3D12 committed/placed resource.
  ID3D12Resource* resource = nullptr;
  /// D3D12MA allocation backing this texture (nullptr for swapchain images).
  D3D12MA::Allocation* allocation = nullptr;
  /// Index into the CBV/SRV/UAV descriptor heap for shader access, or
  /// `DX12_DESCRIPTOR_INDEX_NONE` when this texture is never sampled.
  uint32_t srv_index = DX12_DESCRIPTOR_INDEX_NONE;
  /// Index into the RTV heap, kept so `destroyTexture` can hand it back.
  uint32_t rtv_index = DX12_DESCRIPTOR_INDEX_NONE;
  /// Index into the DSV heap, kept so `destroyTexture` can hand it back.
  uint32_t dsv_index = DX12_DESCRIPTOR_INDEX_NONE;
  /// CPU descriptor handle for render target view (if applicable).
  std::optional<D3D12_CPU_DESCRIPTOR_HANDLE> rtv_handle{};
  /// CPU descriptor handle for depth stencil view (if applicable).
  std::optional<D3D12_CPU_DESCRIPTOR_HANDLE> dsv_handle{};
  /// Texture width in texels.
  uint32_t width = 0;
  /// Texture height in texels.
  uint32_t height = 0;
  /// DXGI pixel format.
  DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
  /// Original RHI format (for updateTexture2D validation).
  RhiFormat rhi_format = RhiFormat::UNDEFINED;
  /// State the resource was last transitioned to. D3D12 has no way to ask
  /// the driver, so every barrier this backend records reads and updates
  /// this field; a stale value produces a debug-layer error, not a guess.
  D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_DX12
