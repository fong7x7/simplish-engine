#pragma once

#include <cstdint>

namespace eng {

struct AuditEmitResult {
  /// Ring buffer offset of the committed event (0 if dropped).
  uint32_t ring_buffer_offset{};
  /// Whether the event was successfully written to the ring buffer.
  bool emitted{};
};

}  // namespace eng
