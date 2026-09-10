#pragma once

// Design Summary -- MeshRenderer
//
// Behaviours:
//   - Owns the backend's static-mesh pipeline and the depth target it needs
//   - Uploads MeshData to GPU vertex and index buffers, keyed by MeshGpuId
//   - Shades each instance with the diffuse map it names, or with a
//     built-in untextured stand-in when it names none
//   - Draws a list of instances inside a render pass the caller opened,
//     shaded by the lights the caller hands it
//
// Edge Cases:
//   - Backend without a mesh pipeline: init() returns false, ready() stays
//     false, and draw() is a no-op. Everything else in the frame still runs
//   - Empty mesh, or a mesh whose buffers failed to allocate: not uploaded,
//     and upload() reports nullopt rather than handing back a broken id
//   - Instance naming an unknown mesh: skipped
//   - Instance naming no texture: drawn with the stand-in, which is one
//     texel of the flat colour meshes had before textures existed. That is
//     what keeps the shader to a single path with no untextured branch
//   - No lights: the draw is lit by one built-in key light, so a scene
//     nobody has lit is readable rather than black
//   - More lights than the shader's loop holds: the ones past
//     MESH_MAX_LIGHTS are dropped, since a shader loop cannot grow
//
// Invariants:
//   - draw() only ever records into a pass the caller began; it never
//     begins or ends one, because the depth target belongs to that pass
//   - The depth target is recreated when the surface size changes, so it
//     always matches the colour attachment
//   - The depth target is sampleable as well as a depth attachment, so a
//     later pass can read what this one wrote — see MeshOutlineRenderer
//
// Integration Points:
//   - RenderedGameClient: opens the scene pass and calls draw()
//   - Editor viewport: supplies the view projection and the instance list
//   - MeshOutlineRenderer: reads depthTarget() after the scene pass ends

#include <cstdint>
#include <engine/math/mat4.h>
#include <engine/render-mesh/mesh-data.h>
#include <engine/render-mesh/mesh-instance.h>
#include <engine/render-mesh/mesh-light.h>
#include <engine/render-mesh/mesh-style.h>
#include <engine/render/rhi-command-list.h>
#include <engine/render/rhi-device.h>
#include <optional>
#include <span>
#include <unordered_map>

namespace eng {

/// Uploads static meshes and draws them with a depth buffer.
/// @thread_safety Main thread only.
class MeshRenderer {
public:
  /// What one draw call needs beyond the instances themselves.
  struct DrawParams {
    /// World-to-clip matrix shared by every instance.
    Mat4 view_projection{};
    /// Instances to draw, in any order — the depth buffer resolves them.
    std::span<const MeshInstance> instances{};
    /// Lights every instance is shaded by. Empty means the built-in key
    /// light, which is what the editor drew with before lights existed.
    std::span<const MeshLight> lights{};
    /// Pixel viewport to draw into.
    RhiViewport viewport{};
    /// Pixel scissor, which keeps meshes inside the editor's viewport rect.
    RhiScissor scissor{};
    /// Tones each light is flattened into — `MeshStyle::shade_bands`.
    /// `MESH_SHADE_SMOOTH` shades exactly as meshes always have.
    uint32_t shade_bands = MESH_SHADE_SMOOTH;
  };

  /// Create the mesh pipeline. False when the backend has none, which
  /// leaves the renderer inert rather than broken.
  bool init(RhiDevice& device);

  /// Release the pipeline, the depth target, the stand-in texture, and
  /// every uploaded mesh.
  void shutdown(RhiDevice& device);

  /// Whether a pipeline exists and draws will do anything.
  [[nodiscard]] bool ready() const { return pipeline_ != RHI_PIPELINE_INVALID; }

  /// Upload a mesh and return its id. Nullopt when it has no triangles or
  /// its buffers could not be allocated.
  std::optional<MeshGpuId> upload(RhiDevice& device, const MeshData& mesh);

  /// The depth texture for a surface of this size, created or resized as
  /// needed. Invalid when the renderer is not ready.
  RhiTextureHandle depthTarget(RhiDevice& device, uint32_t width,
                               uint32_t height);

  /// Record draws for every instance. Must be called inside a render pass
  /// that has this renderer's depth target attached.
  void draw(RhiCommandList& cmd, const DrawParams& params) const;

  /// Number of meshes currently uploaded.
  [[nodiscard]] size_t meshCount() const { return meshes_.size(); }

  /// The one-texel texture an instance naming none is drawn with.
  [[nodiscard]] RhiTextureHandle untexturedStandIn() const {
    return untextured_;
  }

  /// GPU buffers for one uploaded mesh.
  struct GpuMeshBuffers {
    /// Vertex buffer holding `MeshVertex` records.
    RhiBufferHandle vertices = 0;
    /// Index buffer holding 32-bit indices.
    RhiBufferHandle indices = 0;
    /// Number of indices to draw.
    uint32_t index_count = 0;
  };

private:
  /// Record one instance's draw. Separated so `draw` stays a loop.
  void drawInstance(RhiCommandList& cmd, const MeshInstance& instance,
                    const Mat4& view_projection) const;

  /// Create the one-texel stand-in an untextured instance samples.
  bool createUntexturedStandIn(RhiDevice& device);

  /// Hand the fragment stage the lights every instance of this draw is
  /// shaded by, and how many tones to flatten them into, which is one bind
  /// rather than one per instance.
  void bindLights(RhiCommandList& cmd, const DrawParams& params) const;

  /// Release the depth target if one exists.
  void destroyDepthTarget(RhiDevice& device);

  /// The backend's static-mesh pipeline.
  RhiPipelineHandle pipeline_ = RHI_PIPELINE_INVALID;
  /// Uploaded meshes by id.
  std::unordered_map<MeshGpuId, GpuMeshBuffers> meshes_{};
  /// Next id to hand out; starts at 1 so zero stays the invalid sentinel.
  MeshGpuId next_id_ = 1;
  /// Depth texture matching the current surface size.
  RhiTextureHandle depth_target_ = RHI_TEXTURE_INVALID;
  /// Surface width `depth_target_` was created for.
  uint32_t depth_width_ = 0;
  /// Surface height `depth_target_` was created for.
  uint32_t depth_height_ = 0;
  /// One texel of the colour meshes were flat-shaded with before textures,
  /// sampled by every instance that names no map of its own.
  RhiTextureHandle untextured_ = RHI_TEXTURE_INVALID;
};

}  // namespace eng
