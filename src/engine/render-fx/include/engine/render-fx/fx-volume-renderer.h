#pragma once

/// @file fx-volume-renderer.h
/// @brief Marches a ray through every cloud of smoke, over the finished
/// scene and under its particles.
/// @par Threading
/// Main-thread-only.
///
/// The same pass and the same rules as `FxRenderer`: a colour target with
/// no depth attached, the scene's depth read as a texture, premultiplied
/// blending. What differs is what a fragment does with them — rather than
/// fading a flat quad by the depth under it, it steps a ray from the front
/// of the cloud's box until the scene's depth stops it, adding up what the
/// smoke absorbs along the way. That is what lets one cloud fill a doorway
/// and wrap a crate instead of cutting against it.
///
/// It costs a march per covered pixel, so clouds are counted in tens.
/// Draw it before the particles, so sparks show through the smoke they
/// are thrown into.

#include <array>
#include <cstdint>
#include <engine/math/mat4.h>
#include <engine/render-fx/fx-renderer.h>
#include <engine/render-fx/fx-volume-pool.h>
#include <engine/render-fx/fx-volume-vertex.h>
#include <engine/render/rhi-command-list.h>
#include <engine/render/rhi-device.h>
#include <utility>
#include <vector>

namespace eng {

/// Uploads and draws an `FxVolumePool`.
/// @thread_safety Main thread only.
class FxVolumeRenderer {
public:
  /// What one volumetric draw needs; `FxRenderer::DrawParams` with clouds
  /// in place of particles.
  struct DrawParams {
    /// The clouds to draw; nothing is drawn without them.
    const FxVolumePool* volumes = nullptr;
    /// The depth the scene pass wrote. It must not be attached to the
    /// pass this draws in.
    RhiTextureHandle depth = RHI_TEXTURE_INVALID;
    /// World-to-clip matrix the scene was drawn with.
    Mat4 view_projection{};
    /// Pixel viewport the scene drew with, covering the whole surface.
    RhiViewport viewport{};
    /// Scissor the scene drew with; smoke stays inside it.
    RhiScissor scissor{};
  };

  /// Create the pipeline and the vertex buffers, each with room for
  /// @p volumes clouds. False when the backend has no volume pipeline,
  /// which leaves the renderer inert and the scene without smoke.
  bool init(RhiDevice& device, uint32_t volumes = FX_VOLUME_CAPACITY);

  /// Release the pipeline and the buffers.
  void shutdown(RhiDevice& device);

  /// Whether draws will do anything.
  [[nodiscard]] bool ready() const { return pipeline_ != RHI_PIPELINE_INVALID; }

  /// Lay out, upload and draw @p params' clouds, under the same rules as
  /// `FxRenderer::draw`. The nearest clouds are drawn when there are more
  /// than a buffer holds.
  void draw(RhiDevice& device, RhiCommandList& cmd, const DrawParams& params);

  /// Vertices the last draw recorded.
  [[nodiscard]] uint32_t vertexCount() const { return vertex_count_; }

private:
  /// Lay the clouds out and copy them into the next buffer. False when
  /// there is nothing to draw or the buffer could not be written.
  bool upload(RhiDevice& device, const DrawParams& params);

  /// Release every buffer that was made.
  void destroyBuffers(RhiDevice& device);

  /// The backend's volume pipeline.
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
  std::vector<FxVolumeVertex> vertices_;
  /// Scratch for sorting clouds by depth.
  std::vector<std::pair<float, uint32_t>> order_;
};

}  // namespace eng
