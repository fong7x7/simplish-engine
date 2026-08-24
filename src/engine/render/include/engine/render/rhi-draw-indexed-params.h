#pragma once

#include <cstdint>

namespace eng {

/// Parameters for an indexed draw call.
struct RhiDrawIndexedParams {
  /// Number of indices to draw.
  uint32_t index_count = 0;
  /// Number of instances to draw.
  uint32_t instance_count = 1;
  /// Offset into the index buffer.
  uint32_t first_index = 0;
  /// Value added to each vertex index.
  int32_t vertex_offset = 0;
  /// Index of the first instance.
  uint32_t first_instance = 0;
};

}  // namespace eng
