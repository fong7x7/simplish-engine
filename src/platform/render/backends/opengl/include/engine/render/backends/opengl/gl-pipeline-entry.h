#pragma once

#ifdef ENGINE_RENDERER_OPENGL

#include <cstdint>

using GLuint = unsigned int;
using GLint = int;

namespace eng::render {

struct GlPipelineEntry {
  /// Linked GL shader program.
  GLuint program = 0;
  /// Vertex array object encoding vertex layout.
  GLuint vao = 0;
  /// GL primitive topology (GL_TRIANGLES, etc.).
  unsigned int topology = 0;
  /// Interleaved vertex stride for `glBindVertexBuffer` (0 = legacy default).
  uint32_t vertex_stride = 0;
  /// `u_screen_scale` uniform location; -1 if unused.
  GLint loc_u_screen_scale = -1;
  /// Blend state snapshot.
  bool blend_enabled = false;
  /// Depth test enabled.
  bool depth_test = true;
  /// Depth write enabled.
  bool depth_write = true;
  /// Wireframe mode.
  bool wireframe = false;
  /// Back-face culling enabled.
  bool cull_back = true;
  /// Front faces use counter-clockwise winding.
  bool front_ccw = true;
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_OPENGL
