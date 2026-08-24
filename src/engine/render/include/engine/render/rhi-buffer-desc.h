#pragma once

#include "rhi-core-types.h"

#include <cstdint>

namespace eng {

struct RhiBufferDesc {
  /// Buffer size in bytes.
  uint64_t size = 0;
  /// Intended buffer usage flags (vertex, index, uniform, etc.).
  RhiBufferUsage usage = RhiBufferUsage::VERTEX;
  /// Whether the buffer is CPU-mappable for upload/readback.
  bool host_visible = false;
  /// Optional debug label shown in GPU profilers.
  const char* debug_name = nullptr;
};

}  // namespace eng
