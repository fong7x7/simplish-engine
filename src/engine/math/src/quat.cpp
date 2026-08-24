#include <cmath>
#include <engine/math/quat.h>

namespace eng {

Quat operator*(const Quat& a, const Quat& b) {
  return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
          a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
          a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
          a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

float Quat::length(const Quat& q) {
  return std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
}

Quat Quat::normalize(const Quat& q) {
  float len = length(q);
  if (len == 0.0f) {
    return q;
  }
  return {q.x / len, q.y / len, q.z / len, q.w / len};
}

Quat Quat::conjugate(const Quat& q) {
  return {-q.x, -q.y, -q.z, q.w};
}

}  // namespace eng
