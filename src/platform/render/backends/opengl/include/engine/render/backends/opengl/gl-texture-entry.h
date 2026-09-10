#pragma once

#ifdef ENGINE_RENDERER_OPENGL

#include <cstdint>
#include <engine/render/rhi-types.h>

using GLuint = unsigned int;

namespace eng::render {

struct GlTextureEntry {
  /// OpenGL texture object name.
  GLuint gl_id = 0;
  /// Texture width in texels.
  uint32_t width = 0;
  /// Texture height in texels.
  uint32_t height = 0;
  /// Original RHI format.
  RhiFormat format = RhiFormat::UNDEFINED;
  /// Original RHI usage. A sampled depth target is one a pass's depth has
  /// to be copied into, since GL draws depth into the default framebuffer.
  RhiTextureUsage usage = RhiTextureUsage::SAMPLED;
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_OPENGL
