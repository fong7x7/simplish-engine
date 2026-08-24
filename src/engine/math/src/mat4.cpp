#include <engine/math/mat4.h>

namespace eng {

Mat4 Mat4::identity() {
  Mat4 r{};
  r.m[0] = 1;
  r.m[5] = 1;
  r.m[10] = 1;
  r.m[15] = 1;
  return r;
}

Mat4 operator*(const Mat4& a, const Mat4& b) {
  Mat4 r{};
  for (int col = 0; col < 4; col++) {
    for (int row = 0; row < 4; row++) {
      r.m[col * 4 + row] = a.m[0 * 4 + row] * b.m[col * 4 + 0] +
                           a.m[1 * 4 + row] * b.m[col * 4 + 1] +
                           a.m[2 * 4 + row] * b.m[col * 4 + 2] +
                           a.m[3 * 4 + row] * b.m[col * 4 + 3];
    }
  }
  return r;
}

}  // namespace eng
