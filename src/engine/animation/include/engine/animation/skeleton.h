#pragma once

/// @file skeleton.h
/// @brief A joint hierarchy and the pose it stands in when nothing moves it.
/// @par Threading
/// Immutable value type once loaded; read from any thread.

#include <cstdint>
#include <engine/animation/joint-pose.h>
#include <limits>
#include <string>
#include <vector>

namespace eng::animation {

/// `Skeleton::parents` entry for a joint at the top of the hierarchy.
inline constexpr uint32_t SKELETON_NO_PARENT =
    std::numeric_limits<uint32_t>::max();

/// A joint hierarchy, stored as parallel arrays indexed by joint.
///
/// **Every parent precedes its children.** That ordering is what lets one
/// forward pass over the arrays turn local poses into world transforms —
/// each joint's parent is already finished when it is reached — so it is
/// an invariant of the type rather than something a caller sorts for: a
/// loader establishes it, and `skeletonIsOrdered` checks it.
///
/// The joints are every transform a skin's matrices depend on, which can be
/// more than the joints the skin names: an exporter's armature node, sitting
/// above the root bone with a scale or a turn of its own, moves every bone
/// below it and so is a joint here even though no vertex hangs from it.
/// @thread_safety Immutable value type once loaded.
struct Skeleton {
  /// Parent of each joint, or `SKELETON_NO_PARENT` for a root. Always less
  /// than the joint's own index.
  std::vector<uint32_t> parents;
  /// Each joint's local pose when no clip channel drives it.
  std::vector<JointPose> rest;
  /// Each joint's name as the source file wrote it, which is what a tool
  /// shows and what a later attachment point will look a hand up by.
  std::vector<std::string> names;
};

/// Whether @p skeleton's arrays agree in length and every parent precedes
/// its child — the shape every function over a skeleton assumes.
[[nodiscard]] bool skeletonIsOrdered(const Skeleton& skeleton);

}  // namespace eng::animation
