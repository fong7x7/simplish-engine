#pragma once

#include <cstdint>
#include <engine/render/rhi-types.h>

namespace eng::render {

struct GlCmdBindVertexBuffer {
  /// Buffer handle to bind.
  RhiBufferHandle buffer = RHI_BUFFER_INVALID;
  /// Byte offset into the buffer.
  uint64_t offset = 0;
};

}  // namespace eng::render
