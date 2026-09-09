#pragma once

/// @file mesh-data.h
/// @brief CPU-side triangle mesh: vertices, indices, and bounds.

#include <cstdint>
#include <engine/math/vec3.h>
#include <engine/render-mesh/mesh-vertex.h>
#include <filesystem>
#include <string>
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
  /// Material library the source file named, as it wrote it. Empty when it
  /// named none.
  ///
  /// This and `material` are what the file said; `texture_path` is what
  /// that turned out to mean. Reading a material library needs the
  /// directory the mesh came from, so only the loader that touched disk can
  /// fill the third in — see `obj-loader.h`.
  std::string material_library;
  /// First material the source file used, or empty. The first rather than
  /// each: the mesh is drawn with one texture, and splitting a model into a
  /// draw per material is submesh work this does not do yet.
  std::string material;
  /// Image the material names as its diffuse map, resolved against the file
  /// the mesh was read from. Empty when there is none to resolve.
  std::filesystem::path texture_path;
};

}  // namespace eng
