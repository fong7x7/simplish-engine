#pragma once

namespace eng {

/// @thread_safety Immutable value type — safe to use from any thread.
struct Quat {
  /// X imaginary component
  float x = 0.0f;
  /// Y imaginary component
  float y = 0.0f;
  /// Z imaginary component
  float z = 0.0f;
  /// W real component (1.0 = identity rotation)
  float w = 1.0f;

  static float length(const Quat& q);
  static Quat normalize(const Quat& q);
  static Quat conjugate(const Quat& q);
};

Quat operator*(const Quat& a, const Quat& b);

}  // namespace eng
