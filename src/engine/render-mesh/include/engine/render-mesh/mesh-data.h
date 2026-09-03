#pragma once

/// @file mesh-data.h
/// @brief CPU-side triangle mesh: vertices, indices, and bounds.

#include <cstdint>
#include <engine/math/vec3.h>
#include <engine/render-mesh/mesh-vertex.h>
#include <vector>

namespace eng {

/// A loaded triangle mesh before it reaches the GPU.
///
/// Bounds come from the source geometry and are what the editor uses to sit
/// a model on the ground plane and scale it to a tile footprint, so a model
/// authored in any unit lands somewhere sensible.
/// @thread_safety Main thread only (owns heap storage).
struct MeshData {
  /// Interleaved vertices.
  std::vector<MeshVertex> vertices;
  /// Triangle list indices into `vertices`.
  std::vector<uint32_t> indices;
  /// Minimum corner of the axis-aligned bounding box.
  Vec3 min{};
  /// Maximum corner of the axis-aligned bounding box.
  Vec3 max{};
};

}  // namespace eng
