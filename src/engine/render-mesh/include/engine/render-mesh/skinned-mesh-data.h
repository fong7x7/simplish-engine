#pragma once

/// @file skinned-mesh-data.h
/// @brief CPU-side skinned triangle mesh: bind-pose vertices with their
/// joint weights, indices, and bounds.

#include <cstdint>
#include <engine/math/vec3.h>
#include <engine/render-mesh/skinned-mesh-vertex.h>
#include <filesystem>
#include <vector>

namespace eng {

/// A loaded skinned mesh before it reaches the GPU.
///
/// Only the triangles and their weights: which joints the weights refer to,
/// and how those joints move, is the rig's business, not the mesh's. The
/// two meet at draw time, when a rig's skin matrices are handed to the
/// renderer beside this mesh's id.
/// @thread_safety Main thread only (owns heap storage).
struct SkinnedMeshData {
  /// Interleaved vertices, in the bind pose.
  std::vector<SkinnedMeshVertex> vertices;
  /// Triangle list indices into `vertices`.
  std::vector<uint32_t> indices;
  /// Minimum corner of the bind pose's axis-aligned bounding box.
  ///
  /// The bind pose rather than any animated one, because it is the one pose
  /// every clip starts from and the one an editor sizes a placed model by:
  /// a walk that swings an arm past the box should not make the character
  /// shrink to fit it.
  Vec3 min{};
  /// Maximum corner of the bind pose's bounding box.
  Vec3 max{};
  /// Image the mesh's material names as its base colour, resolved against
  /// the file the mesh was read from. Empty when there is none on disk.
  std::filesystem::path texture_path;
};

}  // namespace eng
