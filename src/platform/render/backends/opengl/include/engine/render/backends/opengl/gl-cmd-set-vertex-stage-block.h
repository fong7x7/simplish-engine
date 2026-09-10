#pragma once

#include <cstdint>
#include <vector>

namespace eng::render {

/// A block of bytes for the vertex stage too large to record inline.
///
/// `GlCmdSetVertexStageBytes` carries its payload by value in a fixed array,
/// sized for the mesh shader's two matrices, and every command in the stream
/// is as large as the largest alternative. The skinned mesh's joint palette
/// is thirty times that, so it rides here instead, on the heap, rather than
/// making every recorded draw carry four kilobytes of padding.
struct GlCmdSetVertexStageBlock {
  /// Matches RHI `setVertexStageBytes` slot index.
  uint32_t slot = 0;
  /// The payload, copied at record time so the list owns it.
  std::vector<uint8_t> data{};
};

}  // namespace eng::render
