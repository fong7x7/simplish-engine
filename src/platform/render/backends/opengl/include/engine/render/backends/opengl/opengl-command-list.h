#pragma once

#ifdef ENGINE_RENDERER_OPENGL

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// OpenGlCommandList: Deferred command recording for the OpenGL RHI backend.
//
// Responsibilities:
// - Record RHI commands as GlCommand variants between begin()/end()
// - Provide access to the recorded command vector for replay by
//   OpenGlDevice::submit()
//
// Key Invariants:
// - Commands are recorded, not executed, during begin()/end()
// - The command vector is cleared on begin() for reuse
// - After end(), the command list is sealed and ready for submit()
// - Must not be reused until the next frame (after submit completes)
//
// Threading:
// - All methods are main-thread-only (GL context is single-threaded)
// ============================================================================

#include <engine/render/backends/opengl/opengl-command.h>
#include <engine/render/rhi-command-list.h>
#include <vector>

namespace eng::render {

class OpenGlCommandList final : public RhiCommandList {
public:
  // --- Recording lifecycle ---
  void begin() override;
  void end() override;

  // --- Render pass ---
  void beginRenderPass(const RhiRenderPassBeginInfo& info) override;
  void endRenderPass() override;

  // --- Pipeline binding ---
  void bindPipeline(RhiPipelineHandle pipeline) override;

  // --- Resource binding ---
  void bindVertexBuffer(RhiBufferHandle buffer, uint64_t offset) override;
  void bindIndexBuffer(RhiBufferHandle buffer, uint64_t offset,
                       RhiIndexType index_type) override;
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

  // --- Compute dispatch ---
  void dispatch(uint32_t groups_x, uint32_t groups_y,
                uint32_t groups_z) override;

  // --- Copy commands ---
  void copyBuffer(const RhiCopyBufferParams& params) override;
  void copyTextureToBuffer(RhiTextureHandle src, RhiBufferHandle dst) override;

  // --- Barriers ---
  void textureBarrier(RhiTextureHandle texture, RhiTextureLayout old_layout,
                      RhiTextureLayout new_layout) override;

  /// Read-only access to the recorded command vector for replay.
  const std::vector<GlCommand>& commands() const;

private:
  /// Recorded commands awaiting replay.
  std::vector<GlCommand> commands_{};
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_OPENGL
