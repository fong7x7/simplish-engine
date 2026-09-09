#pragma once

#ifdef ENGINE_RENDERER_DX12

#include "dx12-device-impl.h"

#include <array>
#include <d3d12.h>
#include <engine/render/rhi-command-list.h>

namespace eng::render {

/// Render target views of one pass, sized to D3D12's own attachment limit.
using Dx12RtvArray = std::array<D3D12_CPU_DESCRIPTOR_HANDLE,
                                D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT>;

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// Dx12CommandList: DX12 implementation of the RhiCommandList interface.
//
// Responsibilities:
// - Wrap a borrowed ID3D12GraphicsCommandList for GPU command recording
// - Translate RHI commands (bind, draw, dispatch, barrier) to D3D12 calls
// - Drive render target state transitions from the device's tracked states
//
// Key Invariants:
// - Does NOT own the ID3D12GraphicsCommandList (lifetime managed by
//   Dx12PerFrameData)
// - Must be used between begin()/end() calls
// - draw/drawIndexed must be inside a render pass
// - dispatch must be outside a render pass
// - end() leaves every swapchain back buffer back in PRESENT, which is the
//   state IDXGISwapChain::Present requires
// - All methods are main-thread-only
//
// Threading:
// - Recording is single-threaded (main thread)
// - The underlying command list is executed on the GPU queue separately
// ============================================================================

class Dx12CommandList final : public RhiCommandList {
public:
  /// Constructs a command list wrapping a borrowed command list.
  Dx12CommandList(ID3D12GraphicsCommandList* cmd_list, Dx12Device::Impl& impl);

  ~Dx12CommandList() override = default;

  // --- Recording lifecycle ---
  void begin() override;
  void end() override;

  // --- Render pass ---
  void beginRenderPass(const RhiRenderPassBeginInfo& info) override;
  void endRenderPass() override;

  // --- Pipeline binding ---
  void bindPipeline(RhiPipelineHandle pipeline) override;

  // --- Resource binding ---
  void bindVertexBuffer(RhiBufferHandle buffer, uint64_t offset = 0) override;
  void bindIndexBuffer(RhiBufferHandle buffer, uint64_t offset = 0,
                       RhiIndexType index_type = RhiIndexType::UINT16) override;
  void bindDescriptorSet(uint32_t set_index,
                         RhiDescriptorSetHandle set) override;
  void setVertexStageBytes(const void* data, size_t size,
                           uint32_t slot) override;
  void setFragmentStageBytes(const void* data, size_t size,
                             uint32_t slot) override;
  void bindFragmentTexture(RhiTextureHandle texture, uint32_t slot) override;

  // --- Viewport and scissor ---
  void setViewport(const RhiViewport& viewport) override;
  void setScissor(const RhiScissor& scissor) override;

  // --- Draw commands ---
  void draw(const RhiDrawParams& params) override;
  void drawIndexed(const RhiDrawIndexedParams& params) override;
  void drawIndirect(const RhiDrawIndirectParams& params) override;
  void drawIndexedIndirect(const RhiDrawIndexedIndirectParams& params) override;
  void drawIndexedIndirectCount(
      const RhiDrawIndexedIndirectCountParams& params) override;

  // --- Compute dispatch ---
  void dispatch(uint32_t groups_x, uint32_t groups_y = 1,
                uint32_t groups_z = 1) override;

  // --- Copy commands ---
  void copyBuffer(const RhiCopyBufferParams& params) override;
  void copyTextureToBuffer(RhiTextureHandle src, RhiBufferHandle dst) override;

  // --- Compute texture binding ---
  void bindComputeStorageImage(RhiTextureHandle texture, uint32_t mip_level,
                               uint32_t slot) override;
  void bindComputeSampledTexture(RhiTextureHandle texture,
                                 uint32_t slot) override;

  // --- Barriers ---
  void textureBarrier(RhiTextureHandle texture, RhiTextureLayout old_layout,
                      RhiTextureLayout new_layout) override;
  void computeBarrier() override;
  void bufferBarrier(RhiBufferHandle buffer, RhiBarrierStage src_stage,
                     RhiBarrierStage dst_stage) override;

  /// Returns the underlying ID3D12GraphicsCommandList (for submit).
  ID3D12GraphicsCommandList* nativeCommandList() const;

private:
  /// Fill `out_rtvs` with the pass's usable colour views, transitioning
  /// each into RENDER_TARGET. Returns how many it wrote.
  uint32_t collectRenderTargets(const RhiRenderPassBeginInfo& info,
                                Dx12RtvArray& out_rtvs);
  /// Transition every render target of a pass and bind them.
  void bindRenderTargets(const RhiRenderPassBeginInfo& info);
  /// Apply the pass's clear operations to the bound targets.
  void applyLoadOps(const RhiRenderPassBeginInfo& info);
  /// Copy `size` bytes into this frame's ring and point `root_param` at them.
  void bindStageBytes(uint32_t root_param, const void* data, size_t size);
  /// Put every swapchain back buffer back into the PRESENT state.
  void restorePresentState();

  /// Borrowed D3D12 graphics command list (not owned).
  ID3D12GraphicsCommandList* cmd_list_ = nullptr;
  /// Device implementation for resolving RHI handles to D3D12 objects.
  Dx12Device::Impl& impl_;
  /// Currently bound pipeline's topology (for IASetPrimitiveTopology).
  D3D_PRIMITIVE_TOPOLOGY current_topology_ = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
  /// Currently bound pipeline's vertex stride (for vertex buffer binding).
  uint32_t current_vertex_stride_ = 0;
  /// Whether a graphics root signature is set, which the root-argument
  /// calls require and which only `bindPipeline` can establish.
  bool graphics_root_bound_ = false;
  /// Colour attachments the current pass actually bound, which is what the
  /// clear loop iterates rather than the count the caller asked for.
  uint32_t bound_rtv_count_ = 0;
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_DX12
