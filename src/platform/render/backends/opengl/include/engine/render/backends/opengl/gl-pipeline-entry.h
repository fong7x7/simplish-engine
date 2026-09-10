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
  ///
  /// This and the four below are how a pipeline says what its stage-bytes
  /// payloads mean: the GUI's is a screen scale, the mesh's is two matrices
  /// and a block of lights. Which locations a pipeline has is what the
  /// device dispatches on, so neither pipeline needs a kind tag.
  GLint loc_u_screen_scale = -1;
  /// `u_view_projection` uniform location; -1 if unused.
  GLint loc_u_view_projection = -1;
  /// `u_model` uniform location; -1 if unused.
  GLint loc_u_model = -1;
  /// `u_light_count` uniform location; -1 if unused.
  GLint loc_u_light_count = -1;
  /// `u_lights` uniform array location; -1 if unused.
  GLint loc_u_lights = -1;
  /// `u_shade_bands` uniform location; -1 if unused. Rides in the mesh's
  /// light block, as the second word of its header.
  GLint loc_u_shade_bands = -1;
  /// `u_outline` uniform array location; -1 if unused. The outline
  /// pipeline's whole stage-bytes payload, as three `vec4`s.
  GLint loc_u_outline = -1;
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
