#pragma once

/// @file gui-rect.h
/// @brief Float rectangle for GUI layout, hit testing, and draw submission.

namespace eng {

/// Rectangle in logical pixels (float for layout and subpixel-safe tests).
/// @thread_safety Immutable value type.
struct Rect {
  /// Left edge X coordinate.
  float x = 0.0f;
  /// Top edge Y coordinate.
  float y = 0.0f;
  /// Width in pixels.
  float w = 0.0f;
  /// Height in pixels.
  float h = 0.0f;
};

/// Create a Rect from float coordinates.
/// @thread_safety Thread-safe (pure function).
constexpr Rect makeRect(float x, float y, float w, float h) {
  return {x, y, w, h};
}

/// Create a Rect from integer pixel coordinates (avoids narrowing).
/// @thread_safety Thread-safe (pure function).
constexpr Rect makeIntRect(int x, int y, int w, int h) {
  return {static_cast<float>(x), static_cast<float>(y), static_cast<float>(w),
          static_cast<float>(h)};
}

/// Overlap of two rectangles, or a zero-sized rect when they do not meet.
/// @thread_safety Thread-safe (pure function).
constexpr Rect intersectRects(const Rect& a, const Rect& b) {
  const float x0 = a.x > b.x ? a.x : b.x;
  const float y0 = a.y > b.y ? a.y : b.y;
  const float ax1 = a.x + a.w;
  const float bx1 = b.x + b.w;
  const float ay1 = a.y + a.h;
  const float by1 = b.y + b.h;
  const float x1 = ax1 < bx1 ? ax1 : bx1;
  const float y1 = ay1 < by1 ? ay1 : by1;
  return {x0, y0, x1 > x0 ? x1 - x0 : 0.0f, y1 > y0 ? y1 - y0 : 0.0f};
}

/// Half-open hit test: `[x, x+w)` × `[y, y+h)`.
/// @thread_safety Thread-safe (pure function).
inline bool containsPoint(const Rect& rect, float px, float py) {
  return px >= rect.x && px < rect.x + rect.w && py >= rect.y &&
         py < rect.y + rect.h;
}

}  // namespace eng
