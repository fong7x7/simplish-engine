#pragma once

namespace eng {

/// @thread_safety Immutable value type — safe to use from any thread.
///
/// The arithmetic is `constexpr` and in the header so the compiler can
/// inline it: the actor passes do this arithmetic thousands of times a
/// tick, and a call per `+` was most of what separating a horde cost. The
/// results are unchanged — each is one IEEE operation per component, and
/// FMA contraction is off everywhere (`-ffp-contract=off`), so an inlined
/// `a * b + c` still rounds twice.
struct Vec2 {
  /// X component
  float x = 0.0f;
  /// Y component
  float y = 0.0f;

  static constexpr float dot(const Vec2& a, const Vec2& b) {
    return a.x * b.x + a.y * b.y;
  }
  static constexpr float cross(const Vec2& a, const Vec2& b) {
    return a.x * b.y - a.y * b.x;
  }
  static float length(const Vec2& v);
  static constexpr float lengthSquared(const Vec2& v) { return dot(v, v); }
  static Vec2 normalize(const Vec2& v);
  static float distance(const Vec2& a, const Vec2& b);
  static constexpr float distanceSquared(const Vec2& a, const Vec2& b);
  static Vec2 lerp(const Vec2& a, const Vec2& b, float t);
  static constexpr Vec2 perpendicular(const Vec2& v) { return {-v.y, v.x}; }
};

constexpr Vec2 operator+(const Vec2& a, const Vec2& b) {
  return {a.x + b.x, a.y + b.y};
}
constexpr Vec2 operator-(const Vec2& a, const Vec2& b) {
  return {a.x - b.x, a.y - b.y};
}
constexpr Vec2 operator*(const Vec2& v, float s) {
  return {v.x * s, v.y * s};
}
constexpr Vec2 operator*(float s, const Vec2& v) {
  return {v.x * s, v.y * s};
}
constexpr Vec2 operator/(const Vec2& v, float s) {
  return {v.x / s, v.y / s};
}
constexpr Vec2 operator-(const Vec2& v) {
  return {-v.x, -v.y};
}

constexpr float Vec2::distanceSquared(const Vec2& a, const Vec2& b) {
  return lengthSquared(a - b);
}

}  // namespace eng
