#pragma once

/// @file skinned-mesh-renderer.h
/// @brief Uploads skinned meshes and draws them posed, into the same pass
/// and depth buffer as static meshes.
/// @par Threading Main-thread only.
///
/// The static mesh path's twin, and deliberately a narrow one. ADR-003 keeps
/// the horde as sprites and admits skinned meshes for a handful of
/// characters — players, bosses — so this draws each instance with its own
/// call and its own palette, and makes no attempt at instancing: at a
/// handful, the draw calls are nothing, and a per-instance palette in one
/// constant block is the simplest thing every backend can bind.
///
/// Skinning happens in the vertex stage, from a `SkinPalette` sent as
/// vertex stage bytes beside the matrices a static mesh gets. The fragment
/// stage is the static mesh's own shader, fed the same light block, so a
/// skinned character is lit, banded, and outlined exactly as the props
/// around it are.
///
/// Edge cases follow `MeshRenderer`: a backend with no skinned pipeline
/// leaves this inert and `draw()` a no-op; an empty mesh is not uploaded; an
/// instance naming an unknown mesh is skipped; one naming no texture takes
/// the stand-in.

#include <cstddef>
#include <engine/math/mat4.h>
#include <engine/render-mesh/mesh-instance.h>
#include <engine/render-mesh/mesh-light.h>
#include <engine/render-mesh/mesh-style.h>
#include <engine/render-mesh/skinned-mesh-data.h>
#include <engine/render-mesh/skinned-mesh-instance.h>
#include <engine/render/rhi-command-list.h>
#include <engine/render/rhi-device.h>
#include <optional>
#include <span>
#include <unordered_map>

namespace eng {

/// Uploads skinned meshes and draws them posed.
/// @thread_safety Main thread only.
class SkinnedMeshRenderer {
public:
  /// What one draw call needs beyond the instances themselves — the static
  /// renderer's parameters, with skinned instances in place of static ones.
  struct DrawParams {
    /// World-to-clip matrix shared by every instance.
    Mat4 view_projection{};
    /// Instances to draw, in any order — the depth buffer resolves them.
    std::span<const SkinnedMeshInstance> instances{};
    /// Lights every instance is shaded by. Empty means the built-in key
    /// light.
    std::span<const MeshLight> lights{};
    /// Pixel viewport to draw into.
    RhiViewport viewport{};
    /// Pixel scissor.
    RhiScissor scissor{};
    /// Tones each light is flattened into — `MeshStyle::shade_bands`.
    uint32_t shade_bands = MESH_SHADE_SMOOTH;
  };

  /// Create the skinned pipeline and the untextured stand-in. False when
  /// the backend has no skinned pipeline, which leaves the renderer inert.
  bool init(RhiDevice& device);

  /// Release the pipeline, the stand-in, and every uploaded mesh.
  void shutdown(RhiDevice& device);

  /// Whether a pipeline exists and draws will do anything.
  [[nodiscard]] bool ready() const { return pipeline_ != RHI_PIPELINE_INVALID; }

  /// Upload a skinned mesh and return its id. Nullopt when it has no
  /// triangles or its buffers could not be allocated.
  std::optional<MeshGpuId> upload(RhiDevice& device,
                                  const SkinnedMeshData& mesh);

  /// Record a draw per instance. Must be called inside a render pass with
  /// `MeshRenderer::depthTarget` attached — the same pass the static meshes
  /// draw in, so the two occlude each other correctly.
  void draw(RhiCommandList& cmd, const DrawParams& params) const;

  /// Number of skinned meshes currently uploaded.
  [[nodiscard]] size_t meshCount() const { return meshes_.size(); }

private:
  /// GPU buffers for one uploaded skinned mesh.
  struct GpuBuffers {
    /// Vertex buffer holding `SkinnedMeshVertex` records.
    RhiBufferHandle vertices = 0;
    /// Index buffer holding 32-bit indices.
    RhiBufferHandle indices = 0;
    /// Number of indices to draw.
    uint32_t index_count = 0;
  };

  /// Create both GPU buffers for a mesh. Either handle is zero on failure.
  static GpuBuffers uploadBuffers(RhiDevice& device,
                                  const SkinnedMeshData& mesh);

  /// Record one instance's draw: its matrices, its palette, its texture.
  void drawInstance(RhiCommandList& cmd, const SkinnedMeshInstance& instance,
                    const Mat4& view_projection) const;

  /// The backend's skinned-mesh pipeline.
  RhiPipelineHandle pipeline_ = RHI_PIPELINE_INVALID;
  /// Uploaded meshes by id.
  std::unordered_map<MeshGpuId, GpuBuffers> meshes_{};
  /// Next id to hand out; starts at 1 so zero stays the invalid sentinel.
  MeshGpuId next_id_ = 1;
  /// One texel of the untextured mesh colour, for instances naming no map.
  RhiTextureHandle untextured_ = RHI_TEXTURE_INVALID;
};

}  // namespace eng
