#include "gltf-node-pose.h"

#include <cmath>

namespace eng::gltf {

namespace {

  /// Length of column @p c's first three rows.
  float columnLength(const Mat4& m, size_t c) {
    return std::sqrt(m(0, c) * m(0, c) + m(1, c) * m(1, c) + m(2, c) * m(2, c));
  }

  /// Determinant of @p m's upper 3×3, whose sign says whether it mirrors.
  float basisDeterminant(const Mat4& m) {
    return m(0, 0) * (m(1, 1) * m(2, 2) - m(1, 2) * m(2, 1)) -
           m(0, 1) * (m(1, 0) * m(2, 2) - m(1, 2) * m(2, 0)) +
           m(0, 2) * (m(1, 0) * m(2, 1) - m(1, 1) * m(2, 0));
  }

  // Algorithm: rotation matrix to quaternion (Shepperd's method) — build
  // the quaternion from whichever of w, x, y, z is largest, so the one
  // square root taken is of the largest quantity and never of a near-zero
  // one, where rounding would dominate.
  Quat rotationToQuat(const Mat4& r) {
    const float trace = r(0, 0) + r(1, 1) + r(2, 2);
    if (trace > 0.0f) {
      const float s = std::sqrt(trace + 1.0f) * 2.0f;
      return Quat::normalize({(r(2, 1) - r(1, 2)) / s, (r(0, 2) - r(2, 0)) / s,
                              (r(1, 0) - r(0, 1)) / s, 0.25f * s});
    }
    if (r(0, 0) > r(1, 1) && r(0, 0) > r(2, 2)) {
      const float s = std::sqrt(1.0f + r(0, 0) - r(1, 1) - r(2, 2)) * 2.0f;
      return Quat::normalize({0.25f * s, (r(0, 1) + r(1, 0)) / s,
                              (r(0, 2) + r(2, 0)) / s,
                              (r(2, 1) - r(1, 2)) / s});
    }
    if (r(1, 1) > r(2, 2)) {
      const float s = std::sqrt(1.0f + r(1, 1) - r(0, 0) - r(2, 2)) * 2.0f;
      return Quat::normalize({(r(0, 1) + r(1, 0)) / s, 0.25f * s,
                              (r(1, 2) + r(2, 1)) / s,
                              (r(0, 2) - r(2, 0)) / s});
    }
    const float s = std::sqrt(1.0f + r(2, 2) - r(0, 0) - r(1, 1)) * 2.0f;
    return Quat::normalize({(r(0, 2) + r(2, 0)) / s, (r(1, 2) + r(2, 1)) / s,
                            0.25f * s, (r(1, 0) - r(0, 1)) / s});
  }

  /// The node's 16-number `matrix`, column-major as glTF writes it.
  Mat4 nodeMatrix(const Json& node) {
    Mat4 m = Mat4::identity();
    for (size_t i = 0; i < 16; ++i) {
      m[i] = jsonNumberAt(node, "matrix", i, m[i]);
    }
    return m;
  }

}  // namespace

animation::JointPose decomposeJointMatrix(const Mat4& m) {
  animation::JointPose pose;
  pose.translation = {m(0, 3), m(1, 3), m(2, 3)};
  // A mirroring matrix has a negative scale somewhere; putting it on X is
  // as good as anywhere, and leaves a proper rotation behind.
  const float mirror = basisDeterminant(m) < 0.0f ? -1.0f : 1.0f;
  pose.scale = {columnLength(m, 0) * mirror, columnLength(m, 1),
                columnLength(m, 2)};
  const float scale[3] = {pose.scale.x, pose.scale.y, pose.scale.z};
  Mat4 rotation = Mat4::identity();
  for (size_t c = 0; c < 3; ++c) {
    for (size_t r = 0; r < 3; ++r) {
      rotation(r, c) =
          scale[c] != 0.0f ? m(r, c) / scale[c] : (r == c ? 1.0f : 0.0f);
    }
  }
  pose.rotation = rotationToQuat(rotation);
  return pose;
}

animation::JointPose gltfNodePose(const Json& node) {
  if (jsonArraySize(node, "matrix") == 16) {
    return decomposeJointMatrix(nodeMatrix(node));
  }
  animation::JointPose pose;
  pose.translation = {jsonNumberAt(node, "translation", 0, 0.0f),
                      jsonNumberAt(node, "translation", 1, 0.0f),
                      jsonNumberAt(node, "translation", 2, 0.0f)};
  pose.rotation = Quat::normalize({jsonNumberAt(node, "rotation", 0, 0.0f),
                                   jsonNumberAt(node, "rotation", 1, 0.0f),
                                   jsonNumberAt(node, "rotation", 2, 0.0f),
                                   jsonNumberAt(node, "rotation", 3, 1.0f)});
  pose.scale = {jsonNumberAt(node, "scale", 0, 1.0f),
                jsonNumberAt(node, "scale", 1, 1.0f),
                jsonNumberAt(node, "scale", 2, 1.0f)};
  return pose;
}

}  // namespace eng::gltf
