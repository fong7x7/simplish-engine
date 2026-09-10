#pragma once

/// @file rig.h
/// @brief Everything that moves a skinned mesh: its skeleton, how the mesh
/// hangs from it, and the clips that pose it.
/// @par Threading
/// Immutable value type once loaded; read from any thread.

#include <engine/animation/animation-clip.h>
#include <engine/animation/skeleton.h>
#include <engine/animation/skin.h>
#include <vector>

namespace eng::animation {

/// A skeleton, the skin binding a mesh to it, and the clips authored for
/// it — the parts of a rigged model that are not triangles.
///
/// Presentation, never simulation. A rig poses what the player sees; the
/// tick never reads a joint, so nothing here is under the determinism
/// contract, and clip time may come from a render frame's delta as freely
/// as from the tick (ADR-002, and the ADR-003 amendment that admits
/// skinned meshes).
/// @thread_safety Immutable value type once loaded.
struct Rig {
  /// The joint hierarchy the clips drive.
  Skeleton skeleton;
  /// How the mesh's vertices name those joints.
  Skin skin;
  /// The motions authored for this skeleton, in the order the source file
  /// listed them.
  std::vector<AnimationClip> clips;
};

}  // namespace eng::animation
