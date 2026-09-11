#include <cmath>
#include <engine/math/vec2.h>

namespace eng {

float Vec2::length(const Vec2& v) {
  return std::sqrt(dot(v, v));
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

Vec2 Vec2::lerp(const Vec2& a, const Vec2& b, float t) {
  return a + (b - a) * t;
}

}  // namespace eng
