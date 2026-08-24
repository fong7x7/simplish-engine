#pragma once

namespace eng {

/// @thread_safety Immutable value type — safe to use from any thread.
struct Vec2 {
  /// X component
  float x = 0.0f;
  /// Y component
  float y = 0.0f;

  static float dot(const Vec2& a, const Vec2& b);
  static float cross(const Vec2& a, const Vec2& b);
  static float length(const Vec2& v);
  static float lengthSquared(const Vec2& v);
  static Vec2 normalize(const Vec2& v);
  static float distance(const Vec2& a, const Vec2& b);
  static float distanceSquared(const Vec2& a, const Vec2& b);
  static Vec2 lerp(const Vec2& a, const Vec2& b, float t);
  static Vec2 perpendicular(const Vec2& v);
};

Vec2 operator+(const Vec2& a, const Vec2& b);
Vec2 operator-(const Vec2& a, const Vec2& b);
Vec2 operator*(const Vec2& v, float s);
Vec2 operator*(float s, const Vec2& v);
Vec2 operator/(const Vec2& v, float s);
Vec2 operator-(const Vec2& v);

}  // namespace eng
