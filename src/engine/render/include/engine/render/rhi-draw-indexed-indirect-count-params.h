#pragma once

#include "rhi-core-types.h"

#include <cstdint>

namespace eng {

/// Parameters for a GPU-driven indexed indirect draw with count buffer.
/// @note Threading: Immutable value type.
struct RhiDrawIndexedIndirectCountParams {
  /// Buffer containing indexed draw arguments.
  RhiBufferHandle arg_buffer = RHI_BUFFER_INVALID;
  /// Byte offset into the argument buffer.
  uint64_t arg_offset = 0;
  /// Buffer containing the uint32_t draw count.
  RhiBufferHandle count_buffer = RHI_BUFFER_INVALID;
  /// Byte offset into the count buffer.
  uint64_t count_offset = 0;
  /// Maximum number of draws (upper bound for count buffer value).
  uint32_t max_draw_count = 0;
  /// Byte stride between consecutive draw arguments.
  uint32_t stride = 0;
};

}  // namespace eng
