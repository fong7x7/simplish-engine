#include <algorithm>
#include <cmath>
#include <engine/render-mesh/mesh-outline-renderer.h>
#include <engine/render/rhi-draw-params.h>

namespace eng {

namespace {

  /// Fragment-stage slot the outline shader reads its parameters from.
  constexpr uint32_t OUTLINE_UNIFORM_SLOT = 0;

  /// Fragment-stage slot the outline shader reads the scene's depth from.
  constexpr uint32_t OUTLINE_DEPTH_SLOT = 0;

  /// One triangle large enough to cover the viewport, generated in the
  /// vertex shader from its vertex index, so no vertex buffer is bound.
  constexpr uint32_t FULL_SCREEN_TRIANGLE_VERTICES = 3;

  /// What the fragment stage reads, in the layout every backend's outline
  /// shader declares — `OUTLINE_MSL_SOURCE`, `OUTLINE_HLSL_SOURCE`, and
  /// `OUTLINE_FRAGMENT_SHADER_GLSL` — none of which can include this file.
  struct alignas(16) OutlineUniforms {
    /// Line colour, linear RGBA. Alpha is always one; the shader scales it
    /// by how far past the threshold a pixel is, which softens creases.
    float color[4]{};
    /// The scissor as left, top, right and bottom in surface pixels,
    /// top-left origin, right and bottom exclusive.
    float bounds[4]{};
    /// Distance in whole pixels either side of a pixel that depth is read.
    float width = 1.0f;
    /// Second difference of depth past which a pixel is on an edge.
    float threshold = 0.0f;
    /// The rest of the register.
    float padding[2]{};
  };

  static_assert(sizeof(OutlineUniforms) == 48,
                "the outline shaders read three float4s");

  /// Length of one row of the matrix's upper-left 3x3: how far that clip
  /// axis moves per world unit, in the direction it moves fastest.
  float rowLength(const Mat4& m, size_t row) {
    return std::sqrt(m(row, 0) * m(row, 0) + m(row, 1) * m(row, 1) +
                     m(row, 2) * m(row, 2));
  }

  /// The width depth is sampled at: whole pixels, and never less than one.
  float samplePixels(float width) {
    return std::max(1.0f, std::round(width));
  }

  /// The scissor as the shader's left, top, right and bottom.
  void setBounds(OutlineUniforms& u, const RhiScissor& scissor) {
    u.bounds[0] = static_cast<float>(scissor.x);
    u.bounds[1] = static_cast<float>(scissor.y);
    u.bounds[2] = u.bounds[0] + static_cast<float>(scissor.width);
    u.bounds[3] = u.bounds[1] + static_cast<float>(scissor.height);
  }

  OutlineUniforms toUniforms(const MeshOutlineRenderer::DrawParams& params) {
    OutlineUniforms u;
    u.color[0] = params.color.x;
    u.color[1] = params.color.y;
    u.color[2] = params.color.z;
    u.color[3] = 1.0f;
    setBounds(u, params.scissor);
    u.width = samplePixels(params.width);
    u.threshold = MeshOutlineRenderer::creaseThreshold(
        params.view_projection, params.viewport.width, params.width);
    return u;
  }

}  // namespace

bool MeshOutlineRenderer::init(RhiDevice& device) {
  RhiPipelineHandle pipeline = RHI_PIPELINE_INVALID;
  if (!device.tryCreateMeshOutlinePipeline(pipeline)) {
    return false;
  }
  pipeline_ = pipeline;
  return true;
}

void MeshOutlineRenderer::shutdown(RhiDevice& device) {
  if (pipeline_ != RHI_PIPELINE_INVALID) {
    device.destroyPipeline(pipeline_);
    pipeline_ = RHI_PIPELINE_INVALID;
  }
}

float MeshOutlineRenderer::creaseThreshold(const Mat4& view_projection,
                                           float surface_width, float width) {
  // Clip x spans two units across the surface, so this is surface pixels
  // per world unit; depth per world unit is the depth row's own length.
  const float pixels_per_world =
      rowLength(view_projection, 0) * surface_width * 0.5f;
  if (pixels_per_world <= 0.0f) {
    return 0.0f;
  }
  const float depth_per_world = rowLength(view_projection, 2);
  // A crease changes the slope of depth by this much per world unit, and
  // the samples either side of it are `width` pixels of world apart.
  return MESH_OUTLINE_CREASE_SLOPE * samplePixels(width) * depth_per_world /
         pixels_per_world;
}

void MeshOutlineRenderer::draw(RhiCommandList& cmd,
                               const DrawParams& params) const {
  if (!ready() || params.depth == RHI_TEXTURE_INVALID || params.width <= 0.0f) {
    return;
  }
  cmd.bindPipeline(pipeline_);
  cmd.setViewport(params.viewport);
  cmd.setScissor(params.scissor);
  const OutlineUniforms uniforms = toUniforms(params);
  cmd.setFragmentStageBytes(&uniforms, sizeof(uniforms), OUTLINE_UNIFORM_SLOT);
  cmd.bindFragmentTexture(params.depth, OUTLINE_DEPTH_SLOT);
  RhiDrawParams draw{};
  draw.vertex_count = FULL_SCREEN_TRIANGLE_VERTICES;
  cmd.draw(draw);
}

}  // namespace eng
