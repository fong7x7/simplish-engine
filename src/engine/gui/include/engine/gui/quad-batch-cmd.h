#pragma once

#include <cstdint>

namespace eng {

using RhiTextureHandle = uint64_t;

/// @thread_safety Main thread only.
struct QuadBatchCmd {
  /// Start offset into the vertex buffer.
  uint32_t vertex_offset = 0;
  /// Number of vertices in this batch.
  uint32_t vertex_count = 0;
  /// Start offset into the index buffer.
  uint32_t index_offset = 0;
  /// Number of indices in this batch.
  uint32_t index_count = 0;
  /// Texture bound for this batch (0 = untextured).
  RhiTextureHandle texture = 0;
};

}  // namespace eng
