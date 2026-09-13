#pragma once

/// @file fx-renderer.h
/// @brief Draws particles over the finished scene, hidden and softened by
/// the depth it wrote.
/// @par Threading
/// Main-thread-only.
///
/// Engine REQUIREMENTS §5.3 puts transparent effects after the outline, so
/// no line the outline draws lands on a glow. The outline reads the scene's
/// depth as a texture, in the pass after the scene's, which has no depth
/// attachment — so this does too: each particle's fragment reads the depth
/// under it and compares its own. Behind geometry it is gone; just in front
/// of geometry it fades, over `FX_SOFT_TILES`, so a spark skidding along
/// the floor thins out rather than being cut by it.
///
/// Particles are uploaded every frame into one of `FX_FRAME_BUFFER_COUNT`
/// vertex buffers in turn, as the GUI's are, so the CPU never writes a
/// buffer a frame still in flight is drawing from.

#include <array>
#include <cstdint>
#include <engine/math/mat4.h>
#include <engine/render-fx/fx-particle-pool.h>
#include <engine/render-fx/fx-vertex.h>
#include <engine/render/rhi-command-list.h>
#include <engine/render/rhi-device.h>
#include <utility>
#include <vector>

namespace eng {

/// How many vertex buffers the renderer rotates through: two frames in
/// flight on every backend, and a spare — `GUI_FRAME_BUFFER_COUNT`'s
/// reasoning exactly.
inline constexpr uint32_t FX_FRAME_BUFFER_COUNT = 3;

/// How far in front of geometry, in tiles along the view ray, a particle
/// starts to fade into it.
inline constexpr float FX_SOFT_TILES = 0.35f;

/// Uploads and draws an `FxParticlePool`.
/// @thread_safety Main thread only.
class FxRenderer {
public:
  /// What one effects draw needs.
  struct DrawParams {
    /// The particles to draw; nothing is drawn without them.
    const FxParticlePool* particles = nullptr;
    /// The depth the scene pass wrote — `MeshRenderer::depthTarget`. It
    /// must not be attached to the pass this draws in.
    RhiTextureHandle depth = RHI_TEXTURE_INVALID;
    /// World-to-clip matrix the scene was drawn with.
    Mat4 view_projection{};
    /// Pixel viewport the scene drew with, covering the whole surface.
    RhiViewport viewport{};
    /// Scissor the scene drew with; particles stay inside it.
    RhiScissor scissor{};
  };

  /// Create the pipeline and the vertex buffers, each with room for
  /// @p particles particles. False when the backend has no effects
  /// pipeline, or the buffers could not be made, which leaves the renderer
  /// inert.
  bool init(RhiDevice& device, uint32_t particles = FX_PARTICLE_CAPACITY);

  /// Release the pipeline and the buffers.
  void shutdown(RhiDevice& device);

  /// Whether draws will do anything.
  [[nodiscard]] bool ready() const { return pipeline_ != RHI_PIPELINE_INVALID; }

  /// Lay out, upload and draw @p params' particles. Must be called inside
  /// a pass whose colour target is the scene's and which has no depth
  /// attachment, at most once a frame. Records nothing with no particles,
  /// no depth, or no pipeline. The nearest particles are drawn when there
  /// are more than a buffer holds.
  void draw(RhiDevice& device, RhiCommandList& cmd, const DrawParams& params);

  /// Vertices the last draw recorded.
  [[nodiscard]] uint32_t vertexCount() const { return vertex_count_; }

  /// What the fragment stage multiplies a depth difference by to get how
  /// far into its fade a particle is: one over `FX_SOFT_TILES` in depth
  /// units under @p view_projection. Zero for a matrix with no depth.
  [[nodiscard]] static float softness(const Mat4& view_projection);

private:
  /// Lay out the particles and copy them into the next buffer. False when
  /// there is nothing to draw or the buffer could not be written.
  bool upload(RhiDevice& device, const DrawParams& params);

  /// Release every buffer that was made.
  void destroyBuffers(RhiDevice& device);

  /// The backend's effects pipeline.
  RhiPipelineHandle pipeline_ = RHI_PIPELINE_INVALID;
  /// One vertex buffer per frame in flight, and a spare.
  std::array<RhiBufferHandle, FX_FRAME_BUFFER_COUNT> buffers_{};
  /// Vertices each buffer holds.
  uint32_t capacity_ = 0;
  /// Index into `buffers_` of the one the last draw wrote.
  uint32_t slot_ = 0;
  /// Vertices the last draw recorded.
  uint32_t vertex_count_ = 0;
  /// The frame's vertices, kept so a frame does not allocate.
  std::vector<FxVertex> vertices_;
  /// Scratch for sorting particles by depth.
  std::vector<std::pair<float, uint32_t>> order_;
};

}  // namespace eng
