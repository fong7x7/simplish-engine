#pragma once

/// @file mesh-rasterizer.h
/// @brief A CPU rasterizer for meshes, shaded the way the viewport shades
/// them.
///
/// This exists because the GPU path is not always available. A real Metal
/// device needs a native window, so a headless test only ever gets
/// `MetalStubDevice`, which has no pipelines; and an asset thumbnail is
/// wanted whether or not a mesh pipeline came up at all.
///
/// What it covers is everything fed *to* the GPU path: the view-projection
/// matrix, the depth ordering that oblique projection demands, the
/// placement transform, the lights, and the mesh data itself. If a model
/// lands on the wrong tile, sits through the floor, fails to occlude its
/// neighbour, or is lit from the wrong side, this sees it. The shaders
/// themselves remain the user's to verify by running the editor — this
/// shading is written to match them, not derived from them.
/// @par Threading Thread-safe (pure function over the scene it is given).

#include <cstdint>
#include <engine/gui/image-data.h>
#include <engine/math/mat4.h>
#include <engine/render-mesh/mesh-data.h>
#include <engine/render-mesh/mesh-light.h>
#include <span>
#include <vector>

namespace eng::editor {

/// Everything one CPU render needs.
/// @thread_safety Immutable value type.
struct MeshRasterScene {
  /// One mesh and where to draw it.
  struct Draw {
    /// Geometry to draw; must outlive the render call.
    const MeshData* mesh = nullptr;
    /// Object-to-world transform.
    Mat4 model{};
  };

  /// World-to-clip matrix, from `makeIsoViewProjection`.
  Mat4 view_projection{};
  /// Draws, in any order — the depth buffer resolves them.
  std::span<const Draw> draws{};
  /// Lights to shade by. Empty means the built-in key light, which is what
  /// a thumbnail is rendered with and what a scene with no lights of its
  /// own gets.
  std::span<const MeshLight> lights{};
  /// Output width in pixels.
  uint32_t width = 1;
  /// Output height in pixels.
  uint32_t height = 1;
};

/// Rasterize a scene into an RGBA8 image, depth-tested, with the same
/// directional shading the mesh shader applies.
[[nodiscard]] ImageData rasterizeMeshScene(const MeshRasterScene& scene);

}  // namespace eng::editor
