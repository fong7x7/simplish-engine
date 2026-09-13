#include <algorithm>
#include <cmath>
#include <cstring>
#include <engine/render-fx/fx-quads.h>
#include <engine/render-fx/fx-renderer.h>
#include <engine/render/rhi-buffer-desc.h>
#include <engine/render/rhi-draw-params.h>

namespace eng {

namespace {

  /// Fragment-stage slot the effects shader reads its parameters from.
  constexpr uint32_t FX_UNIFORM_SLOT = 0;

  /// Fragment-stage slot the effects shader reads the scene's depth from.
  constexpr uint32_t FX_DEPTH_SLOT = 0;

  /// What the fragment stage reads, in the layout every backend's effects
  /// shader declares — `FX_MSL_SOURCE`, `FX_HLSL_SOURCE`, and the two GLSL
  /// copies — none of which can include this file.
  struct alignas(16) FxUniforms {
    /// `FxRenderer::softness` for the scene's matrix.
    float softness = 0.0f;
    /// The rest of the register.
    float padding[3]{};
  };

  static_assert(sizeof(FxUniforms) == 16, "the effects shaders read a float4");

  /// A vertex buffer the CPU writes every frame.
  RhiBufferHandle createVertexBuffer(RhiDevice& device, uint32_t vertices) {
    RhiBufferDesc desc{};
    desc.size = static_cast<uint64_t>(vertices) * sizeof(FxVertex);
    desc.usage = RhiBufferUsage::VERTEX;
    desc.host_visible = true;
    desc.debug_name = "fx_vertex_buffer";
    return device.createBuffer(desc);
  }

}  // namespace

bool FxRenderer::init(RhiDevice& device, uint32_t particles) {
  RhiPipelineHandle pipeline = RHI_PIPELINE_INVALID;
  if (particles == 0 || !device.tryCreateFxParticlePipeline(pipeline)) {
    return false;
  }
  capacity_ = particles * FX_VERTICES_PER_PARTICLE;
  for (RhiBufferHandle& buffer : buffers_) {
    buffer = createVertexBuffer(device, capacity_);
    if (buffer == 0) {
      destroyBuffers(device);
      device.destroyPipeline(pipeline);
      return false;
    }
  }
  pipeline_ = pipeline;
  return true;
}

void FxRenderer::shutdown(RhiDevice& device) {
  destroyBuffers(device);
  if (pipeline_ != RHI_PIPELINE_INVALID) {
    device.destroyPipeline(pipeline_);
    pipeline_ = RHI_PIPELINE_INVALID;
  }
}

void FxRenderer::destroyBuffers(RhiDevice& device) {
  for (RhiBufferHandle& buffer : buffers_) {
    if (buffer != 0) {
      device.destroyBuffer(buffer);
      buffer = 0;
    }
  }
  capacity_ = 0;
}

float FxRenderer::softness(const Mat4& view_projection) {
  // Depth is measured along the projection ray (Engine §5.1), so the depth
  // row's own length is the depth one tile along that ray covers.
  const Mat4& m = view_projection;
  const float per_tile =
      std::sqrt(m(2, 0) * m(2, 0) + m(2, 1) * m(2, 1) + m(2, 2) * m(2, 2));
  return per_tile > 0.0f ? 1.0f / (FX_SOFT_TILES * per_tile) : 0.0f;
}

bool FxRenderer::upload(RhiDevice& device, const DrawParams& params) {
  buildFxQuads(
      *params.particles,
      {params.view_projection, params.viewport.width, params.viewport.height},
      order_, vertices_);
  const auto total = static_cast<uint32_t>(vertices_.size());
  const uint32_t count = std::min(total, capacity_);
  slot_ = (slot_ + 1) % FX_FRAME_BUFFER_COUNT;
  void* mapped = count > 0 ? device.mapBuffer(buffers_[slot_]) : nullptr;
  if (mapped == nullptr) {
    return false;
  }
  // Farthest first, so what does not fit is what is farthest away.
  std::memcpy(mapped, vertices_.data() + (total - count),
              static_cast<size_t>(count) * sizeof(FxVertex));
  device.unmapBuffer(buffers_[slot_]);
  vertex_count_ = count;
  return true;
}

void FxRenderer::draw(RhiDevice& device, RhiCommandList& cmd,
                      const DrawParams& params) {
  vertex_count_ = 0;
  if (!ready() || params.particles == nullptr || params.particles->live == 0 ||
      params.depth == RHI_TEXTURE_INVALID || !upload(device, params)) {
    return;
  }
  cmd.bindPipeline(pipeline_);
  cmd.setViewport(params.viewport);
  cmd.setScissor(params.scissor);
  cmd.bindVertexBuffer(buffers_[slot_]);
  const FxUniforms uniforms{softness(params.view_projection)};
  cmd.setFragmentStageBytes(&uniforms, sizeof(uniforms), FX_UNIFORM_SLOT);
  cmd.bindFragmentTexture(params.depth, FX_DEPTH_SLOT);
  RhiDrawParams draw{};
  draw.vertex_count = vertex_count_;
  cmd.draw(draw);
}

}  // namespace eng
