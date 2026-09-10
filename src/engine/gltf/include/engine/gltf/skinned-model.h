#pragma once

/// @file skinned-model.h
/// @brief A rigged model as a loader delivers it: the triangles, and the
/// rig that moves them.
/// @par Threading
/// Immutable value type once loaded; read from any thread.

#include <engine/animation/rig.h>
#include <engine/render-mesh/skinned-mesh-data.h>

namespace eng::gltf {

/// A skinned mesh and its rig, which belong together — the mesh's joint
/// indices mean nothing without the rig's skin, and the rig poses nothing
/// without a mesh — and which go separate ways once loaded: the mesh to
/// `SkinnedMeshRenderer::upload`, the rig to a `RigPose` every frame.
/// @thread_safety Immutable value type once loaded.
struct SkinnedModel {
  /// Bind-pose triangles, each vertex naming up to four of the rig's skin
  /// joints.
  SkinnedMeshData mesh;
  /// Skeleton, skin, and clips.
  animation::Rig rig;
};

}  // namespace eng::gltf
