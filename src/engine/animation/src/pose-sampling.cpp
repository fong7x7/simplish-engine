#include <algorithm>
#include <cmath>
#include <engine/animation/pose-sampling.h>

namespace eng::animation {

namespace {

  /// Above this cosine two rotations are close enough that slerp's divide
  /// by the sine of their angle loses more than it gains, and a normalised
  /// straight blend is indistinguishable from it.
  constexpr float SLERP_NEAR_COSINE = 0.9995f;

  /// Floats between one key's start and the next's: one value, or a cubic
  /// spline key's in-tangent, value, and out-tangent.
  size_t keyStride(const AnimationChannel& channel) {
    const size_t n = channelComponents(channel.target);
    return channel.interpolation == ChannelInterpolation::CUBIC_SPLINE ? 3 * n
                                                                       : n;
  }

  /// Where key @p key's value starts in `values` — past its in-tangent,
  /// for a cubic spline.
  const float* keyValue(const AnimationChannel& channel, size_t key) {
    const size_t n = channelComponents(channel.target);
    const bool cubic =
        channel.interpolation == ChannelInterpolation::CUBIC_SPLINE;
    return channel.values.data() + key * keyStride(channel) + (cubic ? n : 0);
  }

  /// Copy key @p key's value into @p out.
  void copyKey(const AnimationChannel& channel, size_t key,
               std::span<float, 4> out) {
    const float* value = keyValue(channel, key);
    std::copy_n(value, channelComponents(channel.target), out.begin());
  }

  /// Straight-line blend of keys @p key and `key + 1`.
  void lerpKeys(const AnimationChannel& channel, size_t key, float u,
                std::span<float, 4> out) {
    const float* a = keyValue(channel, key);
    const float* b = keyValue(channel, key + 1);
    for (size_t i = 0; i < channelComponents(channel.target); ++i) {
      out[i] = a[i] + (b[i] - a[i]) * u;
    }
  }

  /// Scale a quaternion held as four floats to unit length. A zero one is
  /// left alone, since it names no rotation to preserve.
  void normalizeQuat(std::span<float, 4> q) {
    const float length =
        std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (length > 0.0f) {
      for (float& c : q) {
        c /= length;
      }
    }
  }

  /// Spherical blend of two unit quaternions, along the shorter arc.
  ///
  /// `q` and `-q` are the same rotation, so when the two point away from
  /// each other the second is negated first; blending the long way round
  /// would spin a joint most of a turn to move it a little.
  void slerpQuats(const float* a, const float* b, float u,
                  std::span<float, 4> out) {
    float cosine = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    const float sign = cosine < 0.0f ? -1.0f : 1.0f;
    cosine *= sign;
    float wa = 1.0f - u;
    float wb = u * sign;
    if (cosine < SLERP_NEAR_COSINE) {
      const float angle = std::acos(cosine);
      const float sine = std::sin(angle);
      wa = std::sin((1.0f - u) * angle) / sine;
      wb = std::sin(u * angle) / sine * sign;
    }
    for (size_t i = 0; i < 4; ++i) {
      out[i] = a[i] * wa + b[i] * wb;
    }
    normalizeQuat(out);
  }

  /// A cubic Hermite segment between keys @p key and `key + 1`, @p u of the
  /// way along it. glTF's tangents are per second, so they are scaled by the
  /// segment's length here.
  void hermite(const AnimationChannel& channel, size_t key, float u,
               std::span<float, 4> out) {
    const size_t n = channelComponents(channel.target);
    const float span = channel.times[key + 1] - channel.times[key];
    const float* v0 = keyValue(channel, key);
    const float* v1 = keyValue(channel, key + 1);
    const float* out_tangent = v0 + n;
    const float* in_tangent = v1 - n;
    const float u2 = u * u;
    const float u3 = u2 * u;
    const float h00 = 2.0f * u3 - 3.0f * u2 + 1.0f;
    const float h10 = (u3 - 2.0f * u2 + u) * span;
    const float h01 = -2.0f * u3 + 3.0f * u2;
    const float h11 = (u3 - u2) * span;
    for (size_t i = 0; i < n; ++i) {
      out[i] = h00 * v0[i] + h10 * out_tangent[i] + h01 * v1[i] +
               h11 * in_tangent[i];
    }
  }

  /// The key a time strictly inside a channel's keys follows: the last one
  /// at or before it.
  size_t keyBefore(const AnimationChannel& channel, float seconds) {
    const auto after = std::ranges::upper_bound(channel.times, seconds);
    return static_cast<size_t>(after - channel.times.begin()) - 1;
  }

  /// The channel's value at @p seconds, which lies strictly between its
  /// first and last keys: a blend of the two keys either side of it.
  void blendSegment(const AnimationChannel& channel, float seconds,
                    std::span<float, 4> out) {
    const size_t key = keyBefore(channel, seconds);
    const float t0 = channel.times[key];
    const float u = (seconds - t0) / (channel.times[key + 1] - t0);
    if (channel.interpolation == ChannelInterpolation::STEP) {
      copyKey(channel, key, out);
    } else if (channel.interpolation == ChannelInterpolation::CUBIC_SPLINE) {
      hermite(channel, key, u, out);
      if (channel.target == ChannelTarget::ROTATION) {
        normalizeQuat(out);
      }
    } else if (channel.target == ChannelTarget::ROTATION) {
      slerpQuats(keyValue(channel, key), keyValue(channel, key + 1), u, out);
    } else {
      lerpKeys(channel, key, u, out);
    }
  }

  /// The four floats of the pose part @p target names.
  void readTarget(const JointPose& pose, ChannelTarget target,
                  std::span<float, 4> out) {
    const Vec3& v =
        target == ChannelTarget::TRANSLATION ? pose.translation : pose.scale;
    if (target == ChannelTarget::ROTATION) {
      out[0] = pose.rotation.x;
      out[1] = pose.rotation.y;
      out[2] = pose.rotation.z;
      out[3] = pose.rotation.w;
      return;
    }
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
    out[3] = 0.0f;
  }

  /// Write four floats back into the pose part @p target names.
  void writeTarget(std::span<const float, 4> value, ChannelTarget target,
                   JointPose& pose) {
    if (target == ChannelTarget::ROTATION) {
      pose.rotation = {value[0], value[1], value[2], value[3]};
    } else if (target == ChannelTarget::TRANSLATION) {
      pose.translation = {value[0], value[1], value[2]};
    } else {
      pose.scale = {value[0], value[1], value[2]};
    }
  }

}  // namespace

float loopClipTime(const AnimationClip& clip, double elapsed) {
  if (!(clip.duration > 0.0f)) {
    return 0.0f;
  }
  const double duration = clip.duration;
  double wrapped = std::fmod(elapsed, duration);
  if (wrapped < 0.0) {
    wrapped += duration;
  }
  return static_cast<float>(wrapped);
}

void sampleChannel(const AnimationChannel& channel, float seconds,
                   std::span<float, 4> out) {
  const size_t keys = channel.times.size();
  if (keys == 0 || channel.values.size() < keys * keyStride(channel)) {
    return;
  }
  if (keys == 1 || seconds <= channel.times.front()) {
    copyKey(channel, 0, out);
    return;
  }
  if (seconds >= channel.times.back()) {
    copyKey(channel, keys - 1, out);
    return;
  }
  blendSegment(channel, seconds, out);
}

void samplePose(const Skeleton& skeleton, const AnimationClip& clip,
                float seconds, std::span<JointPose> pose) {
  const size_t count = std::min(skeleton.rest.size(), pose.size());
  std::copy_n(skeleton.rest.begin(), count, pose.begin());
  for (const AnimationChannel& channel : clip.channels) {
    if (channel.joint >= count) {
      continue;
    }
    JointPose& joint = pose[channel.joint];
    float value[4] = {};
    readTarget(joint, channel.target, value);
    sampleChannel(channel, seconds, value);
    writeTarget(value, channel.target, joint);
  }
}

void computeJointWorlds(const Skeleton& skeleton,
                        std::span<const JointPose> pose,
                        std::span<Mat4> worlds) {
  const size_t count =
      std::min({skeleton.parents.size(), pose.size(), worlds.size()});
  for (size_t joint = 0; joint < count; ++joint) {
    const Mat4 local = jointPoseMatrix(pose[joint]);
    const uint32_t parent = skeleton.parents[joint];
    // A parent that does not precede its child breaks the skeleton's one
    // invariant; treating the joint as a root keeps the pass in bounds.
    worlds[joint] = parent < joint ? worlds[parent] * local : local;
  }
}

void computeSkinMatrices(const Skin& skin, std::span<const Mat4> worlds,
                         std::span<Mat4> out) {
  const size_t count = std::min(skin.joints.size(), out.size());
  for (size_t k = 0; k < count; ++k) {
    const uint32_t joint = skin.joints[k];
    if (joint >= worlds.size() || k >= skin.inverse_bind.size()) {
      out[k] = Mat4::identity();
      continue;
    }
    out[k] = skin.root * worlds[joint] * skin.inverse_bind[k];
  }
}

}  // namespace eng::animation
