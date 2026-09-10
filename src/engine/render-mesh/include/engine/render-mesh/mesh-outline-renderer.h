#pragma once

/// @file mesh-outline-renderer.h
/// @brief Draws cel-style outlines over meshes, found in the depth they
/// wrote.
/// @par Threading Main-thread only.
///
/// The outline is a second pass rather than something the mesh shader does,
/// because a fragment shading one surface cannot see where that surface
/// meets another: only the finished depth buffer knows that. So the scene
/// pass ends, and this pass reads its depth back and draws a line wherever
/// the depth bends sharply.
///
/// "Bends" is the second difference of depth across a few pixels. Under the
/// engine's orthographic camera, depth across any flat surface is a linear
/// function of screen position, so the second difference is exactly zero on
/// a plane however steeply it is tilted, and large at a silhouette or a
/// crease. Only the near side of an edge is lined — the object's rim rather
/// than whatever lies behind it — so an outline never spills onto the
/// ground around a prop.
///
/// Depth alone cannot find an edge between two surfaces at the same depth
/// and slope, such as a decal on a wall; that would need normals, and the
/// scene pass writes none. It also sees a sprite only if the sprite writes
/// depth, which ADR-003's sprites do.

#include <engine/math/mat4.h>
#include <engine/math/vec3.h>
#include <engine/render/rhi-command-list.h>
#include <engine/render/rhi-device.h>

namespace eng {

/// How sharply depth must bend, as a change of slope, to count as a crease.
///
/// A slope here is world depth per world unit across the screen. A box's
/// right-angled edge changes slope by more than one, and so is lined; the
/// facets of a sphere change slope by a few hundredths each, and are not.
/// A silhouette is a change of depth rather than of slope, and exceeds any
/// threshold this could be set to.
inline constexpr float MESH_OUTLINE_CREASE_SLOPE = 0.5f;

/// Draws outlines from a finished depth buffer.
/// @thread_safety Main thread only.
class MeshOutlineRenderer {
public:
  /// What one outline pass needs.
  struct DrawParams {
    /// The depth the scene pass wrote — `MeshRenderer::depthTarget`. It
    /// must not be attached to the pass this draws in.
    RhiTextureHandle depth = RHI_TEXTURE_INVALID;
    /// World-to-clip matrix the scene was drawn with, which is what turns
    /// the crease threshold into depth units.
    Mat4 view_projection{};
    /// Thickness in surface pixels — the style's width times the pixel
    /// density. Rounded to whole pixels, since depth is read per pixel.
    float width = 0.0f;
    /// Line colour, linear RGB.
    Vec3 color{};
    /// Pixel viewport the scene drew with, covering the whole surface.
    RhiViewport viewport{};
    /// Scissor the scene drew with. Lines stay inside it, and depth outside
    /// it is never read, so a mesh cut off by the edge of the editor's
    /// viewport is not outlined along the cut.
    RhiScissor scissor{};
  };

  /// Create the outline pipeline. False when the backend has none, which
  /// leaves the renderer inert and meshes unoutlined.
  bool init(RhiDevice& device);

  /// Release the pipeline.
  void shutdown(RhiDevice& device);

  /// Whether a pipeline exists and draws will do anything.
  [[nodiscard]] bool ready() const { return pipeline_ != RHI_PIPELINE_INVALID; }

  /// Record the outline. Must be called inside a pass whose colour target
  /// is the one the scene drew into and which has no depth attachment. A
  /// zero width or an invalid depth texture records nothing.
  void draw(RhiCommandList& cmd, const DrawParams& params) const;

  /// The second difference of depth, across @p width pixels, that a crease
  /// of `MESH_OUTLINE_CREASE_SLOPE` produces under @p view_projection on a
  /// surface @p surface_width pixels wide.
  ///
  /// Measured from the matrix rather than passed in, so the line catches
  /// the same creases at every zoom: zooming in spreads a crease over more
  /// pixels, which lowers its second difference, and lowers this with it.
  [[nodiscard]] static float creaseThreshold(const Mat4& view_projection,
                                             float surface_width, float width);

private:
  /// The backend's outline pipeline.
  RhiPipelineHandle pipeline_ = RHI_PIPELINE_INVALID;
};

}  // namespace eng
