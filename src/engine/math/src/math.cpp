#include <cmath>
#include <engine/math/math.h>
#include <numbers>

namespace eng::math {

namespace {
  constexpr float PI = std::numbers::pi_v<float>;
  constexpr float TWO_PI = 2.0f * PI;
}  // namespace

float smoothstep(float t) {
  t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
  return t * t * (3.0f - 2.0f * t);
}

float wrapAngle(float angle) {
  angle = std::fmod(angle + PI, TWO_PI);
  if (angle < 0.0f) {
    angle += TWO_PI;
  }
  return angle - PI;
}

float distance2d(const Vec2& a, const Vec2& b) {
  return Vec2::distance(a, b);
}

float distance2d(const Vec2& a, const Vec3& b) {
  float x = b.x - a.x;
  float y = b.y - a.y;
  return std::sqrt(x * x + y * y);
}

float distance2d(const Vec3& a, const Vec2& b) {
  float x = b.x - a.x;
  float y = b.y - a.y;
  return std::sqrt(x * x + y * y);
}

float distance2d(const Vec3& a, const Vec3& b) {
  return Vec3::distance2d(a, b);
}

float distance3d(const Vec3& a, const Vec3& b) {
  return Vec3::distance3d(a, b);
}

Vec3 rotate(const Quat& q, const Vec3& v) {
  Quat vq{v.x, v.y, v.z, 0.0f};
  Quat rq = q * vq * Quat::conjugate(q);
  return {rq.x, rq.y, rq.z};
}

Vec3 transformPoint(const Mat4& m, const Vec3& v) {
  return {m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z + m.m[12],
          m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z + m.m[13],
          m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14]};
}

// Algorithm: Normalized quaternion to 3×3 rotation embedded in Mat4 identity.
Mat4 quatToMat4(const Quat& q) {
  Quat n = Quat::normalize(q);

  float xx = n.x * n.x;
  float yy = n.y * n.y;
  float zz = n.z * n.z;

  float xy = n.x * n.y;
  float xz = n.x * n.z;
  float yz = n.y * n.z;

  float wx = n.w * n.x;
  float wy = n.w * n.y;
  float wz = n.w * n.z;

  Mat4 m{};

  m.m[0] = 1 - 2 * (yy + zz);
  m.m[1] = 2 * (xy + wz);
  m.m[2] = 2 * (xz - wy);
  m.m[3] = 0;

  m.m[4] = 2 * (xy - wz);
  m.m[5] = 1 - 2 * (xx + zz);
  m.m[6] = 2 * (yz + wx);
  m.m[7] = 0;

  m.m[8] = 2 * (xz + wy);
  m.m[9] = 2 * (yz - wx);
  m.m[10] = 1 - 2 * (xx + yy);
  m.m[11] = 0;

  m.m[12] = 0;
  m.m[13] = 0;
  m.m[14] = 0;
  m.m[15] = 1;

  return m;
}

}  // namespace eng::math
