#pragma once

namespace eng {

/// @thread_safety Immutable value type — safe to use from any thread.
struct Vec3 {
  /// X component
  float x = 0.0f;
  /// Y component
  float y = 0.0f;
  /// Z component
  float z = 0.0f;

  static float dot(const Vec3& a, const Vec3& b);
  static Vec3 cross(const Vec3& a, const Vec3& b);
  static float length(const Vec3& v);
  static Vec3 normalize(const Vec3& v);
  static float distance2d(const Vec3& a, const Vec3& b);
  static float distance3d(const Vec3& a, const Vec3& b);
};

Vec3 operator+(const Vec3& a, const Vec3& b);
Vec3 operator-(const Vec3& a, const Vec3& b);
Vec3 operator*(const Vec3& v, float s);
Vec3 operator*(float s, const Vec3& v);
Vec3 operator/(const Vec3& v, float s);

}  // namespace eng
