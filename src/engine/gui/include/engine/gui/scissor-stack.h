#pragma once

#include <cstdint>

namespace eng {

constexpr uint32_t MAX_SCISSOR_DEPTH = 16;

/// @thread_safety Main thread only.
struct ScissorStack {
  /// Stack of scissor rectangles, each as (x, y, w, h).
  float rects[MAX_SCISSOR_DEPTH][4]{};
  /// Current stack depth (number of active scissor rects).
  uint32_t depth = 0;
};

}  // namespace eng
