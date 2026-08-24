#include "dx12-command-list.h"

#ifdef ENGINE_RENDERER_DX12

#include "dx12-device-impl.h"
#include "dx12-format-map.h"

#include <cstdint>
#include <d3d12.h>
#include <engine/render/rhi-types.h>

namespace eng::render {

// ---------------------------------------------------------------------------
// Anonymous-namespace helpers for handle resolution
// ---------------------------------------------------------------------------

namespace {

  ID3D12Resource* resolveSwapchainImage(Dx12Device::Impl& impl,
                                        RhiTextureHandle handle) {
    auto sc_count = static_cast<RhiTextureHandle>(DX12_FRAMES_IN_FLIGHT);
    if (handle > 0 && handle <= sc_count) {
      return impl.swapchain_images[handle - 1];
    }
    return nullptr;
  }

  ID3D12Resource* resolveResource(Dx12Device::Impl& impl,
                                  RhiTextureHandle handle) {
    auto* sc = resolveSwapchainImage(impl, handle);
    if (sc != nullptr) {
      return sc;
    }
    auto* tex = impl.textures.lookup(handle);
    return tex != nullptr ? tex->resource : nullptr;
  }

  D3D12_CPU_DESCRIPTOR_HANDLE
  resolveRtv(Dx12Device::Impl& impl, RhiTextureHandle handle) {
    auto sc_count = static_cast<RhiTextureHandle>(DX12_FRAMES_IN_FLIGHT);
    if (handle > 0 && handle <= sc_count) {
      return impl.swapchain_rtvs[handle - 1];
    }
    auto* tex = impl.textures.lookup(handle);
    if (tex != nullptr && tex->rtv_handle.has_value()) {
      return tex->rtv_handle.value();
    }
    return {0};
  }

  D3D12_CPU_DESCRIPTOR_HANDLE
  resolveDsv(Dx12Device::Impl& impl, RhiTextureHandle handle) {
    auto* tex = impl.textures.lookup(handle);
    if (tex != nullptr && tex->dsv_handle.has_value()) {
      return tex->dsv_handle.value();
    }
    return {0};
  }

  D3D12_TEXTURE_COPY_LOCATION
  buildSubresourceCopyLoc(ID3D12Resource* resource) {
    D3D12_TEXTURE_COPY_LOCATION loc{};
    loc.pResource = resource;
    loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    return loc;
  }

  D3D12_TEXTURE_COPY_LOCATION
  buildFootprintCopyLoc(ID3D12Resource* buf_resource,
                        const D3D12_RESOURCE_DESC& tex_desc) {
    D3D12_TEXTURE_COPY_LOCATION loc{};
    loc.pResource = buf_resource;
    loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    loc.PlacedFootprint.Footprint.Format = tex_desc.Format;
    loc.PlacedFootprint.Footprint.Width = static_cast<UINT>(tex_desc.Width);
    loc.PlacedFootprint.Footprint.Height = tex_desc.Height;
    loc.PlacedFootprint.Footprint.Depth = 1;
    loc.PlacedFootprint.Footprint.RowPitch =
        static_cast<UINT>(tex_desc.Width) * 4;
    return loc;
  }

}  // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

Dx12CommandList::Dx12CommandList(ID3D12GraphicsCommandList* cmd_list,
                                 Dx12Device::Impl& impl)
  : cmd_list_(cmd_list), impl_(impl) {}

// ---------------------------------------------------------------------------
// Recording lifecycle
// ---------------------------------------------------------------------------

void Dx12CommandList::begin() {
  // Command list is already reset in beginFrame; set descriptor heaps
  ID3D12DescriptorHeap* heaps[] = {impl_.cbv_srv_uav_heap};
  cmd_list_->SetDescriptorHeaps(1, heaps);
}

void Dx12CommandList::end() {
  cmd_list_->Close();
}

// ---------------------------------------------------------------------------
// Render pass
// ---------------------------------------------------------------------------

void Dx12CommandList::beginRenderPass(const RhiRenderPassBeginInfo& info) {
  // Transition color target to render target state
  if (info.color_target_count > 0 && info.color_targets != nullptr) {
    auto* resource = resolveResource(impl_, info.color_targets[0]);
    if (resource != nullptr) {
      D3D12_RESOURCE_BARRIER barrier{};
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barrier.Transition.pResource = resource;
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
      barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
      barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
      cmd_list_->ResourceBarrier(1, &barrier);
    }
  }

  // Set render targets
  if (info.color_target_count > 0 && info.color_targets != nullptr) {
    auto rtv = resolveRtv(impl_, info.color_targets[0]);
    D3D12_CPU_DESCRIPTOR_HANDLE* dsv_ptr = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE dsv{};
    if (info.depth_target != RHI_TEXTURE_INVALID) {
      dsv = resolveDsv(impl_, info.depth_target);
      dsv_ptr = &dsv;
    }
    cmd_list_->OMSetRenderTargets(1, &rtv, FALSE, dsv_ptr);

    // Clear
    if (info.color_load_op == RhiLoadOp::CLEAR) {
      cmd_list_->ClearRenderTargetView(rtv, info.clear_color, 0, nullptr);
    }
    if (info.depth_target != RHI_TEXTURE_INVALID &&
        info.depth_load_op == RhiLoadOp::CLEAR && dsv_ptr != nullptr) {
      cmd_list_->ClearDepthStencilView(*dsv_ptr, D3D12_CLEAR_FLAG_DEPTH,
                                       info.clear_depth, 0, 0, nullptr);
    }
  }
}

void Dx12CommandList::endRenderPass() {
  // DX12 has no explicit render pass end; transition back to present
  // is done by the caller or in present()
}

// ---------------------------------------------------------------------------
// Pipeline binding
// ---------------------------------------------------------------------------

void Dx12CommandList::bindPipeline(RhiPipelineHandle pipeline) {
  auto* p = impl_.pipelines.lookup(pipeline);
  if (p == nullptr) {
    return;
  }
  cmd_list_->SetPipelineState(p->pipeline_state);
  if (p->bind_point == Dx12PipelineType::GRAPHICS) {
    cmd_list_->SetGraphicsRootSignature(p->root_signature);
    current_topology_ = p->topology;
    current_vertex_stride_ = p->vertex_stride;
  } else {
    cmd_list_->SetComputeRootSignature(p->root_signature);
  }
}

// ---------------------------------------------------------------------------
// Resource binding
// ---------------------------------------------------------------------------

void Dx12CommandList::bindVertexBuffer(RhiBufferHandle buffer,
                                       uint64_t offset) {
  auto* buf = impl_.buffers.lookup(buffer);
  if (buf == nullptr) {
    return;
  }
  D3D12_VERTEX_BUFFER_VIEW view{};
  view.BufferLocation = buf->resource->GetGPUVirtualAddress() + offset;
  view.SizeInBytes = static_cast<UINT>(buf->resource->GetDesc().Width - offset);
  view.StrideInBytes = current_vertex_stride_;
  cmd_list_->IASetVertexBuffers(0, 1, &view);
}

void Dx12CommandList::bindIndexBuffer(RhiBufferHandle buffer, uint64_t offset,
                                      RhiIndexType index_type) {
  auto* buf = impl_.buffers.lookup(buffer);
  if (buf == nullptr) {
    return;
  }
  D3D12_INDEX_BUFFER_VIEW view{};
  view.BufferLocation = buf->resource->GetGPUVirtualAddress() + offset;
  view.SizeInBytes = static_cast<UINT>(buf->resource->GetDesc().Width - offset);
  view.Format = toDxgiIndexFormat(index_type);
  cmd_list_->IASetIndexBuffer(&view);
}

void Dx12CommandList::bindDescriptorSet(uint32_t /*set_index*/,
                                        RhiDescriptorSetHandle /*set*/) {
  // Deferred to render-pipeline Phase 1 when descriptor management is designed.
}

// ---------------------------------------------------------------------------
// Viewport and scissor
// ---------------------------------------------------------------------------

void Dx12CommandList::setViewport(const RhiViewport& viewport) {
  D3D12_VIEWPORT vp{};
  vp.TopLeftX = viewport.x;
  vp.TopLeftY = viewport.y;
  vp.Width = viewport.width;
  vp.Height = viewport.height;
  vp.MinDepth = viewport.min_depth;
  vp.MaxDepth = viewport.max_depth;
  cmd_list_->RSSetViewports(1, &vp);
}

void Dx12CommandList::setScissor(const RhiScissor& scissor) {
  D3D12_RECT rect{};
  rect.left = scissor.x;
  rect.top = scissor.y;
  rect.right = scissor.x + static_cast<LONG>(scissor.width);
  rect.bottom = scissor.y + static_cast<LONG>(scissor.height);
  cmd_list_->RSSetScissorRects(1, &rect);
}

// ---------------------------------------------------------------------------
// Draw commands
// ---------------------------------------------------------------------------

void Dx12CommandList::draw(const RhiDrawParams& params) {
  cmd_list_->IASetPrimitiveTopology(current_topology_);
  cmd_list_->DrawInstanced(params.vertex_count, params.instance_count,
                           params.first_vertex, params.first_instance);
}

void Dx12CommandList::drawIndexed(const RhiDrawIndexedParams& params) {
  cmd_list_->IASetPrimitiveTopology(current_topology_);
  cmd_list_->DrawIndexedInstanced(params.index_count, params.instance_count,
                                  params.first_index, params.vertex_offset,
                                  params.first_instance);
}

// ---------------------------------------------------------------------------
// Indirect draw commands
// ---------------------------------------------------------------------------

void Dx12CommandList::drawIndirect(const RhiDrawIndirectParams& params) {
  auto* buf = impl_.buffers.lookup(params.buffer);
  if (buf == nullptr || impl_.draw_indirect_sig == nullptr) {
    return;
  }
  cmd_list_->IASetPrimitiveTopology(current_topology_);
  cmd_list_->ExecuteIndirect(impl_.draw_indirect_sig, params.draw_count,
                             buf->resource, params.offset, nullptr, 0);
}

void Dx12CommandList::drawIndexedIndirect(
    const RhiDrawIndexedIndirectParams& params) {
  auto* buf = impl_.buffers.lookup(params.buffer);
  if (buf == nullptr || impl_.draw_indexed_indirect_sig == nullptr) {
    return;
  }
  cmd_list_->IASetPrimitiveTopology(current_topology_);
  cmd_list_->ExecuteIndirect(impl_.draw_indexed_indirect_sig, params.draw_count,
                             buf->resource, params.offset, nullptr, 0);
}

void Dx12CommandList::drawIndexedIndirectCount(
    const RhiDrawIndexedIndirectCountParams& params) {
  auto* arg_buf = impl_.buffers.lookup(params.arg_buffer);
  auto* cnt_buf = impl_.buffers.lookup(params.count_buffer);
  if (arg_buf == nullptr || cnt_buf == nullptr) {
    return;
  }
  if (impl_.draw_indexed_indirect_sig == nullptr) {
    return;
  }
  cmd_list_->IASetPrimitiveTopology(current_topology_);
  cmd_list_->ExecuteIndirect(
      impl_.draw_indexed_indirect_sig, params.max_draw_count, arg_buf->resource,
      params.arg_offset, cnt_buf->resource, params.count_offset);
}

// ---------------------------------------------------------------------------
// Compute dispatch
// ---------------------------------------------------------------------------

void Dx12CommandList::dispatch(uint32_t groups_x, uint32_t groups_y,
                               uint32_t groups_z) {
  cmd_list_->Dispatch(groups_x, groups_y, groups_z);
}

// ---------------------------------------------------------------------------
// Copy commands
// ---------------------------------------------------------------------------

void Dx12CommandList::copyBuffer(const RhiCopyBufferParams& params) {
  auto* src = impl_.buffers.lookup(params.src);
  auto* dst = impl_.buffers.lookup(params.dst);
  if (src == nullptr || dst == nullptr) {
    return;
  }
  cmd_list_->CopyBufferRegion(dst->resource, params.dst_offset, src->resource,
                              params.src_offset, params.size);
}

void Dx12CommandList::copyTextureToBuffer(RhiTextureHandle src,
                                          RhiBufferHandle dst) {
  auto* resource = resolveResource(impl_, src);
  auto* buf = impl_.buffers.lookup(dst);
  if (resource == nullptr || buf == nullptr) {
    return;
  }
  auto desc = resource->GetDesc();
  auto src_loc = buildSubresourceCopyLoc(resource);
  auto dst_loc = buildFootprintCopyLoc(buf->resource, desc);
  cmd_list_->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);
}

// ---------------------------------------------------------------------------
// Compute texture binding
// ---------------------------------------------------------------------------

void Dx12CommandList::bindComputeStorageImage(RhiTextureHandle /*texture*/,
                                              uint32_t /*mip_level*/,
                                              uint32_t /*slot*/) {
  // DX12 binds UAVs via descriptor tables and root signatures. Implementation
  // requires per-mip UAV descriptors. Deferred to descriptor management
  // integration.
}

void Dx12CommandList::bindComputeSampledTexture(RhiTextureHandle /*texture*/,
                                                uint32_t /*slot*/) {
  // DX12 binds SRVs via descriptor tables. Deferred to descriptor management
  // integration.
}

// ---------------------------------------------------------------------------
// Barriers
// ---------------------------------------------------------------------------

void Dx12CommandList::computeBarrier() {
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
  barrier.UAV.pResource = nullptr;  // Global UAV barrier
  cmd_list_->ResourceBarrier(1, &barrier);
}

void Dx12CommandList::bufferBarrier(RhiBufferHandle buffer,
                                    RhiBarrierStage /*src_stage*/,
                                    RhiBarrierStage /*dst_stage*/) {
  auto* buf = impl_.buffers.lookup(buffer);
  if (buf == nullptr) {
    return;
  }
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
  barrier.UAV.pResource = buf->resource;
  cmd_list_->ResourceBarrier(1, &barrier);
}

void Dx12CommandList::textureBarrier(RhiTextureHandle texture,
                                     RhiTextureLayout old_layout,
                                     RhiTextureLayout new_layout) {
  auto* resource = resolveResource(impl_, texture);
  if (resource == nullptr) {
    return;
  }
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = resource;
  barrier.Transition.StateBefore = toDx12ResourceState(old_layout);
  barrier.Transition.StateAfter = toDx12ResourceState(new_layout);
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  cmd_list_->ResourceBarrier(1, &barrier);
}

// ---------------------------------------------------------------------------
// Native access
// ---------------------------------------------------------------------------

ID3D12GraphicsCommandList* Dx12CommandList::nativeCommandList() const {
  return cmd_list_;
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_DX12
