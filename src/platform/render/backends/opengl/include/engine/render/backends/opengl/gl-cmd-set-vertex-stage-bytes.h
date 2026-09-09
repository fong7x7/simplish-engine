#pragma once

#include <cstdint>

namespace eng::render {

struct GlCmdSetVertexStageBytes {
  /// Matches RHI `setVertexStageBytes` slot index.
  uint32_t slot = 0;
  /// Byte count (`data` prefix).
  uint32_t size = 0;
  /// Inline payload (max 128 bytes), which is what the mesh shader's two
  /// 4x4 matrices need; the GUI's screen scale uses eight of them.
  uint8_t data[128]{};
};

}  // namespace eng::render
