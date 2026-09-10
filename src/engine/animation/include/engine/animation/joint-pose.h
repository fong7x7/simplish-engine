#pragma once

/// @file joint-pose.h
/// @brief Where one joint sits relative to its parent: translation,
/// rotation, and scale.
/// @par Threading
/// Immutable value type and a pure function.

#include <engine/math/mat4.h>
#include <engine/math/quat.h>
#include <engine/math/vec3.h>

namespace eng::animation {

/// One joint's transform relative to its parent, kept as its three parts
/// rather than as a matrix because that is what a clip animates: a channel
/// replaces one part and leaves the other two where they were, and two
/// rotations blend properly only as quaternions.
/// @thread_safety Immutable value type.
struct JointPose {
  /// Offset from the parent joint, in the parent's space.
  Vec3 translation{};
  /// Unit quaternion, applied after the scale and before the translation.
  Quat rotation{};
  /// Per-axis scale, applied first.
  Vec3 scale{1.0f, 1.0f, 1.0f};
};

/// The matrix @p pose describes: translation times rotation times scale,
/// which is the order glTF composes a node's parts in.
[[nodiscard]] Mat4 jointPoseMatrix(const JointPose& pose);

}  // namespace eng::animation
