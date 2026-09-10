#pragma once

/// @file animation-channel.h
/// @brief One animated property of one joint: keyframe times and values.
/// @par Threading
/// Immutable value type once loaded; read from any thread.

#include <cstdint>
#include <vector>

namespace eng::animation {

/// Which part of a joint's pose a channel drives.
enum class ChannelTarget : uint8_t {
  /// `JointPose::translation`: three floats per key.
  TRANSLATION,
  /// `JointPose::rotation`: four floats per key, a quaternion in x, y, z, w.
  ROTATION,
  /// `JointPose::scale`: three floats per key.
  SCALE,
};

/// How a channel moves between two keys.
enum class ChannelInterpolation : uint8_t {
  /// Holds each key's value until the next key's time.
  STEP,
  /// Straight between keys; rotations take the shorter arc at constant
  /// angular speed.
  LINEAR,
  /// A cubic Hermite curve through each key, whose tangents the keys carry.
  CUBIC_SPLINE,
};

/// Number of floats a key of @p target holds.
[[nodiscard]] constexpr uint32_t channelComponents(ChannelTarget target) {
  return target == ChannelTarget::ROTATION ? 4U : 3U;
}

/// One property of one joint over time — glTF's animation channel and its
/// sampler, folded together since nothing here shares a sampler between
/// channels.
/// @thread_safety Immutable value type once loaded.
struct AnimationChannel {
  /// Skeleton joint this drives.
  uint32_t joint = 0;
  /// Which part of that joint's pose.
  ChannelTarget target = ChannelTarget::TRANSLATION;
  /// How values between keys are found.
  ChannelInterpolation interpolation = ChannelInterpolation::LINEAR;
  /// Key times in seconds, strictly ascending.
  std::vector<float> times;
  /// Key values, `channelComponents(target)` floats per key. A cubic spline
  /// key is three of those in a row — the in-tangent, the value, and the
  /// out-tangent — as glTF lays it out.
  std::vector<float> values;
};

}  // namespace eng::animation
