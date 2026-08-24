#pragma once

#include "rhi-types.h"

#include <cstdint>

namespace eng {

/// Parameters for a buffer-to-buffer copy.
struct RhiCopyBufferParams {
  /// Source buffer handle.
  RhiBufferHandle src = RHI_BUFFER_INVALID;
  /// Destination buffer handle.
  RhiBufferHandle dst = RHI_BUFFER_INVALID;
  /// Number of bytes to copy.
  uint64_t size = 0;
  /// Byte offset in the source buffer.
  uint64_t src_offset = 0;
  /// Byte offset in the destination buffer.
  uint64_t dst_offset = 0;
};

}  // namespace eng
