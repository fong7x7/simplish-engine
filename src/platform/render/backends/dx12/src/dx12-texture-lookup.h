#pragma once

#ifdef ENGINE_RENDERER_DX12

/// @file dx12-texture-lookup.h
/// @brief Resolving a texture handle to its D3D12 resource, views and state.
/// @par Threading Main-thread-only, like the device that owns the tables.

#include "dx12-device-impl.h"

#include <d3d12.h>
#include <engine/render/rhi-types.h>

namespace eng::render {

// ---------------------------------------------------------------------------
// Two kinds of texture handle reach these calls. A swapchain back buffer is
// handed out by `backbufferTexture()` as its image index plus one, so 1 and
// 2; everything else comes from the handle table, which encodes a non-zero
// generation in the high 32 bits and so is always at least 2^32. The ranges
// cannot collide, which is what lets one handle type address both.
// ---------------------------------------------------------------------------

/// Whether this handle names a swapchain back buffer rather than a texture.
bool dx12IsSwapchainHandle(RhiTextureHandle handle);

/// Resource behind a texture handle, or null if the handle names nothing.
ID3D12Resource* dx12TextureResource(Dx12Device::Impl& impl,
                                    RhiTextureHandle handle);

/// The tracked resource state for a handle, or null if it names nothing.
///
/// Writable so a barrier can record where it left the resource: D3D12 keeps
/// no state the backend can read back, so this field is the only record.
D3D12_RESOURCE_STATES* dx12TextureStateSlot(Dx12Device::Impl& impl,
                                            RhiTextureHandle handle);

/// Render target view for a handle; `.ptr` is 0 when it has none.
D3D12_CPU_DESCRIPTOR_HANDLE dx12TextureRtv(Dx12Device::Impl& impl,
                                           RhiTextureHandle handle);

/// Depth stencil view for a handle; `.ptr` is 0 when it has none.
D3D12_CPU_DESCRIPTOR_HANDLE dx12TextureDsv(Dx12Device::Impl& impl,
                                           RhiTextureHandle handle);

/// Record a transition to `after`, or nothing if the resource is there
/// already. Updates the tracked state.
void dx12TransitionTexture(ID3D12GraphicsCommandList* list,
                           Dx12Device::Impl& impl, RhiTextureHandle handle,
                           D3D12_RESOURCE_STATES after);

}  // namespace eng::render

#endif  // ENGINE_RENDERER_DX12
