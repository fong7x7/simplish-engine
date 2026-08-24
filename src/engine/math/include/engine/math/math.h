#pragma once

#include <engine/math/mat4.h>
#include <engine/math/quat.h>
#include <engine/math/vec2.h>
#include <engine/math/vec3.h>

namespace eng::math {

/// Clamp a value to the range [lo, hi].
inline float clamp(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

/// Hermite smoothstep: t' = 3t^2 - 2t^3, clamped to [0, 1].
float smoothstep(float t);

/// Wrap angle to [-pi, pi] using fmod (safe for inf/large values).
float wrapAngle(float angle);

float distance2d(const Vec2& a, const Vec2& b);
float distance2d(const Vec2& a, const Vec3& b);
float distance2d(const Vec3& a, const Vec2& b);
float distance2d(const Vec3& a, const Vec3& b);
float distance3d(const Vec3& a, const Vec3& b);

Vec3 rotate(const Quat& q, const Vec3& v);
Vec3 transformPoint(const Mat4& m, const Vec3& v);
Mat4 quatToMat4(const Quat& q);

}  // namespace eng::math
