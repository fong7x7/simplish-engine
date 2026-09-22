#pragma once

/// @file sprite-uv-rect.h
/// @brief The part of a sprite sheet one frame covers.
/// @par Threading Thread-safe (immutable value type).

namespace eng {

/// One frame's rectangle in the sheet's texture coordinates, origin at the
/// top-left of the image as `MeshVertex::uv` is.
/// @thread_safety Immutable value type.
struct SpriteUvRect {
  /// Left edge.
  float u0 = 0.0f;
  /// Top edge.
  float v0 = 0.0f;
  /// Right edge.
  float u1 = 1.0f;
  /// Bottom edge.
  float v1 = 1.0f;
};

}  // namespace eng
