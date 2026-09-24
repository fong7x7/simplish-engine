#pragma once

/// @file water-renderer.h
/// @brief Draws the surface of painted water over the ground: translucent,
/// shaded by its ripples, depth, colour and clarity and the scene's lights.
/// @par Threading
/// Main-thread-only.
///
/// The surface is drawn in the scene pass, after every opaque mesh, against
/// the scene's depth: tested so a crate standing in a pond hides it, and
/// not written, so the outline and the effects still see the ground under
/// it. It is blended premultiplied over the terrain the ground drew under
/// it: the water's colour covers as much of that as its depth and opacity
/// say, the sky it reflects covers more at a glance, and the light it
/// focuses and the glints off it are added. It is lit by the lights the
/// meshes are, in the mesh shader's own block at fragment slot 1.
///
/// Its one texture is the ripple field (`water-texels.h`), rewritten every
/// frame into the next of `WATER_FIELD_TEXTURE_COUNT` textures in turn, so
/// a frame the GPU is still reading is never written under it.

#include <array>
#include <cstdint>
#include <engine/math/mat4.h>
#include <engine/render-mesh/mesh-data.h>
#include <engine/render-mesh/mesh-light.h>
#include <engine/render-mesh/mesh-style.h>
#include <engine/render-water/water-fidelity.h>
#include <engine/render-water/water-field.h>
#include <engine/render-water/water-look.h>
#include <engine/render-water/water-shading.h>
#include <engine/render/rhi-command-list.h>
#include <engine/render/rhi-device.h>
#include <span>
#include <vector>

namespace eng {

/// How many field textures are written in turn: one per frame in flight,
/// and a spare, as the effects' vertex buffers are.
inline constexpr uint32_t WATER_FIELD_TEXTURE_COUNT = 3;

/// Owns the water pipeline, the surface's buffers and the field's
/// textures, and draws the surface.
/// @thread_safety Main thread only.
class WaterRenderer {
public:
  /// What one draw needs beyond the surface and the field already handed
  /// over.
  struct DrawParams {
    /// World-to-clip, the scene's.
    Mat4 view_projection{};
    /// Pixel viewport the scene drew with.
    RhiViewport viewport{};
    /// Scissor the scene drew with; the water stays inside it.
    RhiScissor scissor{};
    /// The colours to shade with.
    WaterLook look{};
    /// The lights the water is lit by, as the scene's meshes are. Empty
    /// means the built-in key light (`makeMeshFragmentLights`).
    std::span<const MeshLight> lights{};
    /// Tones each light is flattened into, as the meshes' are —
    /// `MeshStyle::shade_bands`.
    uint32_t shade_bands = MESH_SHADE_SMOOTH;
    /// How much detail to shade with: `FLAT` draws a still surface.
    WaterFidelity fidelity = WATER_DEFAULT_FIDELITY;
    /// Where the wind waves are in their motion: the frame's clock, not
    /// the tick's.
    float seconds = 0.0f;
  };

  /// Create the pipeline. False when the backend has none, which leaves
  /// the renderer inert and the water flat.
  bool init(RhiDevice& device);

  /// Release the pipeline, the surface and the field.
  void shutdown(RhiDevice& device);

  /// Whether a pipeline exists and draws can do anything.
  [[nodiscard]] bool ready() const { return pipeline_ != RHI_PIPELINE_INVALID; }

  /// Replace the surface with @p mesh — `makeWaterSurfaceMesh`'s. An empty
  /// mesh leaves no surface. False when its buffers could not be made,
  /// which also leaves none.
  bool setSurface(RhiDevice& device, const MeshData& mesh);

  /// Write @p field into the next field texture, making the textures anew
  /// when its size has changed. An empty field leaves none, and the
  /// surface is not drawn. False when a texture could not be made or
  /// written.
  bool setField(RhiDevice& device, const WaterField& field);

  /// Record the surface's draw into the scene pass the caller has open,
  /// after its opaque meshes. Nothing without a surface or a field.
  void draw(RhiCommandList& cmd, const DrawParams& params) const;

  /// Whether the next draw will record anything.
  [[nodiscard]] bool drawable() const;

  /// Indices the surface is drawn with; 0 when there is none.
  [[nodiscard]] uint32_t indexCount() const { return index_count_; }

  /// The field texture the next draw reads; invalid when there is none.
  [[nodiscard]] RhiTextureHandle fieldTexture() const {
    return textures_[slot_];
  }

private:
  /// Release the surface's buffers.
  void releaseSurface(RhiDevice& device);

  /// Release every field texture.
  void releaseField(RhiDevice& device);

  /// Make the field textures @p width × @p height, unless they already
  /// are. False when one could not be made, leaving none.
  bool sizeField(RhiDevice& device, uint32_t width, uint32_t height);

  /// The fragment block for one draw.
  [[nodiscard]] WaterShading shadingFor(const DrawParams& params) const;

  /// Hand the stages this draw's three blocks: the matrix and where the
  /// field lies, the look, and the lights.
  void pushBlocks(RhiCommandList& cmd, const DrawParams& params) const;

  /// The backend's water pipeline.
  RhiPipelineHandle pipeline_ = RHI_PIPELINE_INVALID;
  /// The surface's `MeshVertex` records.
  RhiBufferHandle vertices_ = 0;
  /// The surface's 32-bit indices.
  RhiBufferHandle indices_ = 0;
  /// How many indices the surface is drawn with.
  uint32_t index_count_ = 0;
  /// The field textures, written in turn.
  std::array<RhiTextureHandle, WATER_FIELD_TEXTURE_COUNT> textures_{};
  /// Index into `textures_` of the one the last `setField` wrote.
  uint32_t slot_ = 0;
  /// Width of the field textures, in texels.
  uint32_t texture_width_ = 0;
  /// Height of the field textures, in texels.
  uint32_t texture_height_ = 0;
  /// Where the field lies: its corner and one over its size, in tiles.
  float field_[4]{};
  /// The field's texels, kept so a frame does not allocate.
  std::vector<uint8_t> texels_;
};

}  // namespace eng
