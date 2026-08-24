#pragma once

#include "rhi-core-types.h"

#include <cstdint>

namespace eng {

/// Parameters for an indexed indirect draw call.
/// @note Threading: Immutable value type.
struct RhiDrawIndexedIndirectParams {
  /// Buffer containing indexed draw arguments.
  RhiBufferHandle buffer = RHI_BUFFER_INVALID;
  /// Byte offset into the argument buffer.
  uint64_t offset = 0;
  /// Number of draw commands to execute.
  uint32_t draw_count = 0;
  /// Byte stride between consecutive draw arguments.
  uint32_t stride = 0;
};

}  // namespace eng
