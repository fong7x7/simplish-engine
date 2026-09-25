#pragma once

/// @file water-renderer.h
/// @brief Draws the surface of painted water over the ground: translucent,
/// shaded by its ripples, depth, colour and clarity and the scene's lights.
/// @par Threading
/// Main-thread-only.
///
/// The surface is drawn once the scene pass has ended, in the pass drawn
/// over it, as the effects are: the scene's colour is copied out first
/// (`captureScene`), and the water is drawn over the backbuffer from that
/// copy and the scene's depth. Where something stands in front of the
/// water the depth says so, and the water is not drawn there. Everywhere
/// else, the ground the copy holds is seen through the water — bent by
/// its ripples, each of red, green and blue fading into the water's own
/// colour at its own rate — and the sky and the scene are reflected off
/// it, the light it focuses is laid on the ground, and foam and glints go
/// on top. It is lit by the lights the meshes are, in the mesh shader's
/// own block at fragment slot 1.
///
/// Its four textures, at fragment slots 0 to 3: the ripple field
/// (`writeWaterTexels`), rewritten every frame into the next of
/// `WATER_FIELD_TEXTURE_COUNT` textures in turn, so a frame the GPU is
/// still reading is never written under it; the copy of the scene; the
/// scene's depth; and what does not move about the field
/// (`writeWaterStillTexels`), rewritten only when the water is reshaped.

#include <array>
#include <cstdint>
#include <engine/math/mat4.h>
#include <engine/render-mesh/mesh-data.h>
#include <engine/render-mesh/mesh-light.h>
#include <engine/render-mesh/mesh-style.h>
#include <engine/render-water/water-effects.h>
#include <engine/render-water/water-fidelity.h>
#include <engine/render-water/water-field.h>
#include <engine/render-water/water-look.h>
#include <engine/render-water/water-scene-copy.h>
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
    /// The scene's depth, as the pass it was drawn in left it.
    RhiTextureHandle depth = RHI_TEXTURE_INVALID;
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
    /// Which of the switchable effects to draw, whatever the fidelity.
    WaterEffects effects{};
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

  /// Write @p field's motion into the next field texture, making the
  /// textures anew when its size has changed. An empty field leaves none,
  /// and the surface is not drawn. False when a texture could not be made
  /// or written.
  bool setField(RhiDevice& device, const WaterField& field);

  /// Write what does not move about @p field — where its shore is and which
  /// way it flows — once it has been shaped anew. False when there are no
  /// textures for it or the write failed.
  bool setShape(RhiDevice& device, const WaterField& field);

  /// Copy the scene out of @p copy's source, which the scene pass has
  /// finished drawing into, making the copy anew when its size or format
  /// has changed. Recorded outside any pass. False when there is nothing
  /// to copy into.
  bool captureScene(RhiDevice& device, RhiCommandList& cmd,
                    const WaterSceneCopy& copy);

  /// Record the surface's draw into the pass the caller has open over the
  /// scene. Nothing without a surface, a field, a copy of the scene, or
  /// its depth.
  void draw(RhiCommandList& cmd, const DrawParams& params) const;

  /// Whether the next draw will record anything.
  [[nodiscard]] bool drawable() const;

  /// Indices the surface is drawn with; 0 when there is none.
  [[nodiscard]] uint32_t indexCount() const { return index_count_; }

  /// The field texture the next draw reads; invalid when there is none.
  [[nodiscard]] RhiTextureHandle fieldTexture() const {
    return textures_[slot_];
  }

  /// The texture of what does not move about the field; invalid when
  /// there is none.
  [[nodiscard]] RhiTextureHandle stillTexture() const { return still_; }

  /// The copy of the scene the next draw reads; invalid when there is none.
  [[nodiscard]] RhiTextureHandle sceneTexture() const { return scene_; }

private:
  /// Release the surface's buffers.
  void releaseSurface(RhiDevice& device);

  /// Release every field texture and the still one.
  void releaseField(RhiDevice& device);

  /// Release the copy of the scene.
  void releaseScene(RhiDevice& device);

  /// Make the field textures as big as @p field, unless they already are,
  /// the still one written from it. False when one could not be made,
  /// leaving none.
  bool sizeField(RhiDevice& device, const WaterField& field);

  /// Make the field textures and the still one for @p field. False when
  /// one could not be made.
  bool createFieldTextures(RhiDevice& device, const WaterField& field);

  /// Bind the four textures the fragment stage reads.
  void bindTextures(RhiCommandList& cmd, RhiTextureHandle depth) const;

  /// The fragment block for one draw.
  [[nodiscard]] WaterShading shadingFor(const DrawParams& params) const;

  /// Where the viewport and the copy of the scene lie, and the field's
  /// texel size, into @p shading.
  void placeScreen(WaterShading& shading, const RhiViewport& viewport) const;

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
  /// What does not move about the field.
  RhiTextureHandle still_ = RHI_TEXTURE_INVALID;
  /// The copy of the scene the water is seen through.
  RhiTextureHandle scene_ = RHI_TEXTURE_INVALID;
  /// What the copy of the scene was made for: its size and format.
  WaterSceneCopy scene_shape_{};
  /// Index into `textures_` of the one the last `setField` wrote.
  uint32_t slot_ = 0;
  /// Width of the field textures, in texels.
  uint32_t texture_width_ = 0;
  /// Height of the field textures, in texels.
  uint32_t texture_height_ = 0;
  /// Where the field lies: its corner and one over its size, in tiles.
  float field_[4]{};
  /// One over the field's width and height in samples, then over its
  /// samples a tile.
  float texel_[3]{};
  /// The field's texels, kept so a frame does not allocate.
  std::vector<uint8_t> texels_;
};

}  // namespace eng
