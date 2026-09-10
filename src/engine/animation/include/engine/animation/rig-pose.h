#pragma once

/// @file rig-pose.h
/// @brief The working storage for posing one rig, and the one call that
/// takes a clip and a time to the matrices a skinned draw needs.
/// @par Threading
/// Main-thread only (owns mutable storage).

#include <cstddef>
#include <engine/animation/joint-pose.h>
#include <engine/animation/rig.h>
#include <engine/math/mat4.h>
#include <limits>
#include <span>
#include <vector>

namespace eng::animation {

/// `RigPose::evaluate`'s clip index for "no clip": the skeleton's rest pose.
inline constexpr size_t RIG_REST_POSE = std::numeric_limits<size_t>::max();

/// Every joint's local pose @p seconds into clip @p clip of @p rig, or its
/// rest pose when @p clip is past the rig's clips — `RIG_REST_POSE` among
/// them. @p pose must be as long as the skeleton.
void sampleRigPose(const Rig& rig, size_t clip, float seconds,
                   std::span<JointPose> pose);

/// Storage for posing a rig: local poses, world matrices, and skin matrices,
/// kept between calls so that posing a character every frame allocates
/// only the first time, or when it is handed a bigger rig.
/// @thread_safety Main thread only.
class RigPose {
public:
  /// Pose @p rig @p seconds into clip @p clip and return its skin matrices,
  /// one per skin joint, valid until the next call.
  ///
  /// A clip index past the rig's clips — `RIG_REST_POSE` among them — poses
  /// the skeleton at rest, which for a well-formed rig makes every skin
  /// matrix the identity and draws the mesh as it was modelled. The time is
  /// used as given; `loopClipTime` is what wraps it into the clip.
  std::span<const Mat4> evaluate(const Rig& rig, size_t clip, float seconds);

  /// Skin matrices for local poses the caller built — a blend of two clips,
  /// typically — rather than for one clip. @p locals is copied, one pose per
  /// skeleton joint; a short span leaves the joints past its end at rest.
  std::span<const Mat4> evaluateLocals(const Rig& rig,
                                       std::span<const JointPose> locals);

  /// Every joint's local pose from the last evaluation, which is the pose on
  /// screen: what a crossfade interrupted part way freezes and fades from.
  /// Empty before the first.
  [[nodiscard]] std::span<const JointPose> localPoses() const {
    return locals_;
  }

  /// The world transform of every skeleton joint from the last `evaluate`,
  /// in the skeleton's space before the skin's root is applied.
  [[nodiscard]] std::span<const Mat4> jointWorlds() const { return worlds_; }

private:
  /// Size the three arrays for @p rig.
  void reserveFor(const Rig& rig);

  /// World and skin matrices from `locals_`, which the caller has filled.
  std::span<const Mat4> finish(const Rig& rig);

  /// Local pose per skeleton joint.
  std::vector<JointPose> locals_;
  /// World transform per skeleton joint.
  std::vector<Mat4> worlds_;
  /// Skin matrix per skin joint — what `evaluate` returns.
  std::vector<Mat4> skin_;
};

}  // namespace eng::animation
