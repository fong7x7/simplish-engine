#pragma once

#include <cstdint>

namespace eng::render {

struct GlCmdSetVertexStageBytes {
  /// Matches RHI `setVertexStageBytes` slot index.
  uint32_t slot = 0;
  /// Byte count (`data` prefix).
  uint32_t size = 0;
  /// Inline payload (max 64 bytes).
  uint8_t data[64]{};
};

}  // namespace eng::render
