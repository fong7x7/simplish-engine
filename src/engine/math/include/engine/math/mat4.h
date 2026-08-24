#pragma once

#include <cstddef>

namespace eng {

/// @thread_safety Immutable value type — safe to use from any thread.
struct Mat4 {
  /// Column-major 4x4 matrix elements
  float m[16]{0};

  /// Access raw element (0..15)
  float& operator[](size_t i) { return m[i]; }
  /// Access raw element (0..15) — const
  const float& operator[](size_t i) const { return m[i]; }

  /// Access column pointer (mat.column(col)[row])
  float* column(size_t col) { return &m[col * 4]; }
  /// Access column pointer — const
  const float* column(size_t col) const { return &m[col * 4]; }

  /// Access element by (row, col)
  float& operator()(size_t row, size_t col) { return m[col * 4 + row]; }
  /// Access element by (row, col) — const
  const float& operator()(size_t row, size_t col) const {
    return m[col * 4 + row];
  }

  static Mat4 identity();
};

Mat4 operator*(const Mat4& a, const Mat4& b);

}  // namespace eng
