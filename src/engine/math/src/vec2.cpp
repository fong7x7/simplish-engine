#include <cmath>
#include <engine/math/vec2.h>

namespace eng {

Vec2 operator+(const Vec2& a, const Vec2& b) {
  return {a.x + b.x, a.y + b.y};
}

Vec2 operator-(const Vec2& a, const Vec2& b) {
  return {a.x - b.x, a.y - b.y};
}

Vec2 operator*(const Vec2& v, float s) {
  return {v.x * s, v.y * s};
}

Vec2 operator*(float s, const Vec2& v) {
  return v * s;
}

Vec2 operator/(const Vec2& v, float s) {
  return {v.x / s, v.y / s};
}

Vec2 operator-(const Vec2& v) {
  return {-v.x, -v.y};
}

float Vec2::dot(const Vec2& a, const Vec2& b) {
  return a.x * b.x + a.y * b.y;
}

float Vec2::cross(const Vec2& a, const Vec2& b) {
  return a.x * b.y - a.y * b.x;
}

float Vec2::length(const Vec2& v) {
  return std::sqrt(dot(v, v));
}

float Vec2::lengthSquared(const Vec2& v) {
  return dot(v, v);
}

Vec2 Vec2::normalize(const Vec2& v) {
  float len = length(v);
  if (len == 0.0f) {
    return v;
  }
  return v / len;
}

float Vec2::distance(const Vec2& a, const Vec2& b) {
  return length(a - b);
}

float Vec2::distanceSquared(const Vec2& a, const Vec2& b) {
  return lengthSquared(a - b);
}

Vec2 Vec2::lerp(const Vec2& a, const Vec2& b, float t) {
  return a + (b - a) * t;
}

Vec2 Vec2::perpendicular(const Vec2& v) {
  return {-v.y, v.x};
}

}  // namespace eng
