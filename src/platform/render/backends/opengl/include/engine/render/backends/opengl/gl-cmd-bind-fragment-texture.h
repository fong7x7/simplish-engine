#pragma once

#include <engine/render/rhi-types.h>

namespace eng::render {

struct GlCmdBindFragmentTexture {
  /// Texture handle; invalid clears the binding.
  RhiTextureHandle texture = RHI_TEXTURE_INVALID;
  /// Texture image unit index (`GL_TEXTURE0 + unit`).
  uint32_t unit = 0;
};

}  // namespace eng::render
