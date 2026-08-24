#pragma once

#include <engine/render/rhi-types.h>

namespace eng::render {

struct GlCmdTextureBarrier {
  /// Texture handle.
  RhiTextureHandle texture = RHI_TEXTURE_INVALID;
  /// Previous layout (informational; GL ignores).
  RhiTextureLayout old_layout = RhiTextureLayout::UNDEFINED;
  /// New layout (informational; GL ignores).
  RhiTextureLayout new_layout = RhiTextureLayout::UNDEFINED;
};

}  // namespace eng::render
