#include "dx12-texture-lookup.h"

#ifdef ENGINE_RENDERER_DX12

namespace eng::render {

namespace {

  /// Zero-based swapchain image index for a swapchain handle.
  uint32_t swapchainIndex(RhiTextureHandle handle) {
    return static_cast<uint32_t>(handle) - 1;
  }

  D3D12_RESOURCE_BARRIER buildTransition(ID3D12Resource* resource,
                                         D3D12_RESOURCE_STATES before,
                                         D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    return barrier;
  }

}  // namespace

bool dx12IsSwapchainHandle(RhiTextureHandle handle) {
  return handle > 0 &&
         handle <= static_cast<RhiTextureHandle>(DX12_FRAMES_IN_FLIGHT);
}

ID3D12Resource* dx12TextureResource(Dx12Device::Impl& impl,
                                    RhiTextureHandle handle) {
  if (dx12IsSwapchainHandle(handle)) {
    return impl.swapchain_images[swapchainIndex(handle)];
  }
  auto* tex = impl.textures.lookup(handle);
  return tex != nullptr ? tex->resource : nullptr;
}

D3D12_RESOURCE_STATES* dx12TextureStateSlot(Dx12Device::Impl& impl,
                                            RhiTextureHandle handle) {
  if (dx12IsSwapchainHandle(handle)) {
    return &impl.swapchain_states[swapchainIndex(handle)];
  }
  auto* tex = impl.textures.lookup(handle);
  return tex != nullptr ? &tex->state : nullptr;
}

D3D12_CPU_DESCRIPTOR_HANDLE dx12TextureRtv(Dx12Device::Impl& impl,
                                           RhiTextureHandle handle) {
  if (dx12IsSwapchainHandle(handle)) {
    return impl.swapchain_rtvs[swapchainIndex(handle)];
  }
  auto* tex = impl.textures.lookup(handle);
  if (tex != nullptr && tex->rtv_handle.has_value()) {
    return tex->rtv_handle.value();
  }
  return {0};
}

D3D12_CPU_DESCRIPTOR_HANDLE dx12TextureDsv(Dx12Device::Impl& impl,
                                           RhiTextureHandle handle) {
  auto* tex = impl.textures.lookup(handle);
  if (tex != nullptr && tex->dsv_handle.has_value()) {
    return tex->dsv_handle.value();
  }
  return {0};
}

void dx12TransitionTexture(ID3D12GraphicsCommandList* list,
                           Dx12Device::Impl& impl, RhiTextureHandle handle,
                           D3D12_RESOURCE_STATES after) {
  auto* state = dx12TextureStateSlot(impl, handle);
  auto* resource = dx12TextureResource(impl, handle);
  if (state == nullptr || resource == nullptr || *state == after) {
    return;
  }
  auto barrier = buildTransition(resource, *state, after);
  list->ResourceBarrier(1, &barrier);
  *state = after;
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_DX12
