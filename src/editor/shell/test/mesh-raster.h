#pragma once

/// @file mesh-raster.h
/// @brief Test support: a CPU rasterizer for placed meshes.
///
/// A real Metal device needs a native window, so a headless test only ever
/// gets `MetalStubDevice`, which has no pipelines. Nothing about the GPU
/// path can be exercised here.
///
/// What this rasterizer does cover is everything fed *to* that path: the
/// view-projection matrix, the depth ordering that oblique projection
/// demands, the placement transform, and the mesh data itself. If a model
/// lands on the wrong tile, sits through the floor, or fails to occlude its
/// neighbour, this sees it. The shaders remain the user's to verify by
/// running the editor.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <engine/gui/image-data.h>
#include <engine/math/mat4.h>
#include <engine/render-mesh/mesh-data.h>
#include <span>
#include <vector>

namespace eng::editor::test {

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
  /// Output width in pixels.
  uint32_t width = 1;
  /// Output height in pixels.
  uint32_t height = 1;
};

/// Rasterize a scene into an RGBA8 image, depth-tested, with the same
/// directional shading the mesh shader applies.
[[nodiscard]] ImageData rasterizeMeshScene(const MeshRasterScene& scene);

}  // namespace eng::editor::test
