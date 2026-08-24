#pragma once

#ifdef ENGINE_RENDERER_OPENGL

#include <cstdint>

using GLuint = unsigned int;

namespace eng::render {

struct GlBufferEntry {
  /// OpenGL buffer object name.
  GLuint gl_id = 0;
  /// Buffer size in bytes.
  uint64_t size = 0;
  /// Persistent mapped pointer (nullptr if unmapped).
  void* mapped_ptr = nullptr;
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_OPENGL
