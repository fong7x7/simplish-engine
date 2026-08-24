#pragma once

#include <cstdint>

namespace eng {

/// Parameters for a non-indexed draw call.
struct RhiDrawParams {
  /// Number of vertices to draw.
  uint32_t vertex_count = 0;
  /// Number of instances to draw.
  uint32_t instance_count = 1;
  /// Index of the first vertex.
  uint32_t first_vertex = 0;
  /// Index of the first instance.
  uint32_t first_instance = 0;
};

}  // namespace eng
