#include "dx12-command-list.h"

#ifdef ENGINE_RENDERER_DX12

#include "dx12-copy-alignment.h"
#include "dx12-device-impl.h"
#include "dx12-format-map.h"
#include "dx12-root-signature.h"
#include "dx12-texture-lookup.h"

#include <array>
#include <cstdint>
#include <d3d12.h>
#include <engine/render/rhi-types.h>

namespace eng::render {

// ---------------------------------------------------------------------------
// Anonymous-namespace helpers
// ---------------------------------------------------------------------------

namespace {

  /// Most colour attachments one pass can bind, matching D3D12's own limit.
  constexpr uint32_t DX12_MAX_COLOR_TARGETS =
      D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;

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
    const auto width = static_cast<UINT>(tex_desc.Width);
    D3D12_TEXTURE_COPY_LOCATION loc{};
    loc.pResource = buf_resource;
    loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    loc.PlacedFootprint.Footprint.Format = tex_desc.Format;
    loc.PlacedFootprint.Footprint.Width = width;
    loc.PlacedFootprint.Footprint.Height = tex_desc.Height;
    loc.PlacedFootprint.Footprint.Depth = 1;
    loc.PlacedFootprint.Footprint.RowPitch = dx12AlignRowPitch(width * 4);
    return loc;
  }

  D3D12_RESOURCE_BARRIER buildUavBarrier(ID3D12Resource* resource) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = resource;
    return barrier;
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
  std::array<ID3D12DescriptorHeap*, 1> heaps{impl_.cbv_srv_uav_heap};
  cmd_list_->SetDescriptorHeaps(1, heaps.data());
}

void Dx12CommandList::end() {
  restorePresentState();
  cmd_list_->Close();
}

void Dx12CommandList::restorePresentState() {
  for (uint32_t i = 0; i < DX12_FRAMES_IN_FLIGHT; ++i) {
    dx12TransitionTexture(cmd_list_, impl_, i + 1,
                          D3D12_RESOURCE_STATE_PRESENT);
  }
}

// ---------------------------------------------------------------------------
// Render pass
// ---------------------------------------------------------------------------

uint32_t
Dx12CommandList::collectRenderTargets(const RhiRenderPassBeginInfo& info,
                                      Dx12RtvArray& out_rtvs) {
  const uint32_t requested = info.color_target_count < DX12_MAX_COLOR_TARGETS
                                 ? info.color_target_count
                                 : DX12_MAX_COLOR_TARGETS;
  uint32_t count = 0;
  for (uint32_t i = 0; i < requested; ++i) {
    auto rtv = dx12TextureRtv(impl_, info.color_targets[i]);
    if (rtv.ptr == 0) {
      break;  // A target with no view ends the run; D3D12 rejects a gap.
    }
    dx12TransitionTexture(cmd_list_, impl_, info.color_targets[i],
                          D3D12_RESOURCE_STATE_RENDER_TARGET);
    out_rtvs[count] = rtv;
    ++count;
  }
  return count;
}

void Dx12CommandList::bindRenderTargets(const RhiRenderPassBeginInfo& info) {
  Dx12RtvArray rtvs{};
  const uint32_t count = collectRenderTargets(info, rtvs);
  D3D12_CPU_DESCRIPTOR_HANDLE dsv{};
  if (info.depth_target != RHI_TEXTURE_INVALID) {
    dx12TransitionTexture(cmd_list_, impl_, info.depth_target,
                          D3D12_RESOURCE_STATE_DEPTH_WRITE);
    dsv = dx12TextureDsv(impl_, info.depth_target);
  }
  bound_rtv_count_ = count;
  cmd_list_->OMSetRenderTargets(count, rtvs.data(), FALSE,
                                dsv.ptr != 0 ? &dsv : nullptr);
}

void Dx12CommandList::applyLoadOps(const RhiRenderPassBeginInfo& info) {
  if (info.color_load_op == RhiLoadOp::CLEAR) {
    for (uint32_t i = 0; i < bound_rtv_count_; ++i) {
      auto rtv = dx12TextureRtv(impl_, info.color_targets[i]);
      cmd_list_->ClearRenderTargetView(rtv, info.clear_color, 0, nullptr);
    }
  }
  const auto dsv = dx12TextureDsv(impl_, info.depth_target);
  if (dsv.ptr == 0 || info.depth_load_op != RhiLoadOp::CLEAR) {
    return;
  }
  cmd_list_->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH,
                                   info.clear_depth, info.clear_stencil, 0,
                                   nullptr);
}

void Dx12CommandList::beginRenderPass(const RhiRenderPassBeginInfo& info) {
  if (info.color_target_count == 0 || info.color_targets == nullptr) {
    return;
  }
  bindRenderTargets(info);
  applyLoadOps(info);
}

void Dx12CommandList::endRenderPass() {
  // Nothing to close: D3D12 has no render pass object here, and the back
  // buffer returns to PRESENT in end(). Leaving the target in
  // RENDER_TARGET is what lets the next pass load rather than clear it.
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
  if (p->bind_point != Dx12PipelineType::GRAPHICS) {
    cmd_list_->SetComputeRootSignature(p->root_signature);
    return;
  }
  cmd_list_->SetGraphicsRootSignature(p->root_signature);
  graphics_root_bound_ = true;
  current_topology_ = p->topology;
  current_vertex_stride_ = p->vertex_stride;
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

void Dx12CommandList::bindStageBytes(uint32_t root_param, const void* data,
                                     size_t size) {
  if (root_param == DX12_ROOT_PARAM_NONE || !graphics_root_bound_) {
    return;
  }
  auto& ring = impl_.frames[impl_.frame_index].stage_bytes;
  const auto address = ring.push(data, size);
  if (address == 0) {
    return;
  }
  cmd_list_->SetGraphicsRootConstantBufferView(root_param, address);
}

void Dx12CommandList::setVertexStageBytes(const void* data, size_t size,
                                          uint32_t slot) {
  bindStageBytes(dx12VertexCbvRootParam(slot), data, size);
}

void Dx12CommandList::setFragmentStageBytes(const void* data, size_t size,
                                            uint32_t slot) {
  bindStageBytes(dx12PixelCbvRootParam(slot), data, size);
}

void Dx12CommandList::bindFragmentTexture(RhiTextureHandle texture,
                                          uint32_t slot) {
  auto* tex = impl_.textures.lookup(texture);
  if (slot != 0 || !graphics_root_bound_) {
    return;
  }
  uint32_t srv = impl_.null_srv_index;
  if (tex != nullptr && tex->srv_index != DX12_DESCRIPTOR_INDEX_NONE) {
    dx12TransitionTexture(cmd_list_, impl_, texture,
                          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    srv = tex->srv_index;
  }
  cmd_list_->SetGraphicsRootDescriptorTable(DX12_ROOT_PARAM_PIXEL_SRV_TABLE,
                                            impl_.srvGpuHandle(srv));
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
  if (current_topology_ == D3D_PRIMITIVE_TOPOLOGY_UNDEFINED) {
    return;
  }
  cmd_list_->IASetPrimitiveTopology(current_topology_);
  cmd_list_->DrawInstanced(params.vertex_count, params.instance_count,
                           params.first_vertex, params.first_instance);
}

void Dx12CommandList::drawIndexed(const RhiDrawIndexedParams& params) {
  if (current_topology_ == D3D_PRIMITIVE_TOPOLOGY_UNDEFINED) {
    return;
  }
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
  auto* resource = dx12TextureResource(impl_, src);
  auto* buf = impl_.buffers.lookup(dst);
  if (resource == nullptr || buf == nullptr) {
    return;
  }
  dx12TransitionTexture(cmd_list_, impl_, src,
                        D3D12_RESOURCE_STATE_COPY_SOURCE);
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
  auto barrier = buildUavBarrier(nullptr);
  cmd_list_->ResourceBarrier(1, &barrier);
}

void Dx12CommandList::bufferBarrier(RhiBufferHandle buffer,
                                    RhiBarrierStage /*src_stage*/,
                                    RhiBarrierStage /*dst_stage*/) {
  auto* buf = impl_.buffers.lookup(buffer);
  if (buf == nullptr) {
    return;
  }
  auto barrier = buildUavBarrier(buf->resource);
  cmd_list_->ResourceBarrier(1, &barrier);
}

void Dx12CommandList::textureBarrier(RhiTextureHandle texture,
                                     RhiTextureLayout /*old_layout*/,
                                     RhiTextureLayout new_layout) {
  // The caller's `old_layout` is advisory: the backend already knows where
  // it left the resource, and a barrier whose StateBefore disagrees with
  // the driver's is a debug-layer error rather than a no-op.
  dx12TransitionTexture(cmd_list_, impl_, texture,
                        toDx12ResourceState(new_layout));
}

// ---------------------------------------------------------------------------
// Native access
// ---------------------------------------------------------------------------

ID3D12GraphicsCommandList* Dx12CommandList::nativeCommandList() const {
  return cmd_list_;
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_DX12
