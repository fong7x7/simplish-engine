#include <cmath>
#include <engine/math/vec3.h>

namespace eng {

Vec3 operator+(const Vec3& a, const Vec3& b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 operator-(const Vec3& a, const Vec3& b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 operator*(const Vec3& v, float s) {
  return {v.x * s, v.y * s, v.z * s};
}

Vec3 operator*(float s, const Vec3& v) {
  return v * s;
}

Vec3 operator/(const Vec3& v, float s) {
  return {v.x / s, v.y / s, v.z / s};
}

float Vec3::dot(const Vec3& a, const Vec3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 Vec3::cross(const Vec3& a, const Vec3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

float Vec3::length(const Vec3& v) {
  return std::sqrt(dot(v, v));
}

Vec3 Vec3::normalize(const Vec3& v) {
  float len = length(v);
  if (len == 0.0f) {
    return v;
  }
  return v / len;
}

float Vec3::distance2d(const Vec3& a, const Vec3& b) {
  float dx = b.x - a.x;
  float dy = b.y - a.y;
  return std::sqrt(dx * dx + dy * dy);
}

float Vec3::distance3d(const Vec3& a, const Vec3& b) {
  float dx = b.x - a.x;
  float dy = b.y - a.y;
  float dz = b.z - a.z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

}  // namespace eng
