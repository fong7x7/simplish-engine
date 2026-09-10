#pragma once

/// @file skin.h
/// @brief How a mesh hangs from a skeleton: which joints its vertices name,
/// and where each joint stood when the mesh was bound to it.
/// @par Threading
/// Immutable value type once loaded; read from any thread.

#include <cstdint>
#include <engine/math/mat4.h>
#include <vector>

namespace eng::animation {

/// The binding between a skinned mesh and its skeleton.
///
/// A vertex names joints by their index *here*, not in the skeleton: entry
/// `k` of `joints` is the skeleton joint a vertex naming joint `k` follows.
/// That indirection is glTF's, and it keeps the per-draw palette as short as
/// the skin rather than as long as the skeleton, which matters because the
/// palette travels to the GPU with every draw.
/// @thread_safety Immutable value type once loaded.
struct Skin {
  /// Skeleton joint each palette entry follows.
  std::vector<uint32_t> joints;
  /// Per palette entry, the inverse of that joint's world transform at bind
  /// time: what takes a vertex from the mesh's space into the joint's.
  std::vector<Mat4> inverse_bind;
  /// Transform applied above every root joint, taking the skeleton's space
  /// into the mesh's. Identity unless the mesh was re-oriented after
  /// loading — see `orientSkinnedYUpToZUp` — since turning the vertices
  /// alone would leave the bones pulling them back the old way up.
  Mat4 root = Mat4::identity();
};

}  // namespace eng::animation
