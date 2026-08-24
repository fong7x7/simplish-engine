#pragma once

#include <engine/render/rhi-types.h>

namespace eng::render {

struct GlCmdCopyTextureToBuffer {
  /// Source texture handle.
  RhiTextureHandle src = RHI_TEXTURE_INVALID;
  /// Destination buffer handle.
  RhiBufferHandle dst = RHI_BUFFER_INVALID;
};

}  // namespace eng::render
