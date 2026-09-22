#include <algorithm>
#include <cstring>
#include <engine/render-fx/fx-volume-quads.h>
#include <engine/render-fx/fx-volume-renderer.h>
#include <engine/render/rhi-buffer-desc.h>
#include <engine/render/rhi-draw-params.h>

namespace eng {

namespace {

  /// Fragment-stage slot the volume shader reads the scene's depth from,
  /// the same one the particle shader uses.
  constexpr uint32_t FX_VOLUME_DEPTH_SLOT = 0;

  /// A vertex buffer the CPU writes every frame.
  RhiBufferHandle createVertexBuffer(RhiDevice& device, uint32_t vertices) {
    RhiBufferDesc desc{};
    desc.size = static_cast<uint64_t>(vertices) * sizeof(FxVolumeVertex);
    desc.usage = RhiBufferUsage::VERTEX;
    desc.host_visible = true;
    desc.debug_name = "fx_volume_vertex_buffer";
    return device.createBuffer(desc);
  }

}  // namespace

bool FxVolumeRenderer::init(RhiDevice& device, uint32_t volumes) {
  RhiPipelineHandle pipeline = RHI_PIPELINE_INVALID;
  if (volumes == 0 || !device.tryCreateFxVolumePipeline(pipeline)) {
    return false;
  }
  capacity_ = volumes * FX_VERTICES_PER_VOLUME;
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

void FxVolumeRenderer::shutdown(RhiDevice& device) {
  destroyBuffers(device);
  if (pipeline_ != RHI_PIPELINE_INVALID) {
    device.destroyPipeline(pipeline_);
    pipeline_ = RHI_PIPELINE_INVALID;
  }
}

void FxVolumeRenderer::destroyBuffers(RhiDevice& device) {
  for (RhiBufferHandle& buffer : buffers_) {
    if (buffer != 0) {
      device.destroyBuffer(buffer);
      buffer = 0;
    }
  }
  capacity_ = 0;
}

bool FxVolumeRenderer::upload(RhiDevice& device, const DrawParams& params) {
  buildFxVolumeQuads(
      *params.volumes,
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
              static_cast<size_t>(count) * sizeof(FxVolumeVertex));
  device.unmapBuffer(buffers_[slot_]);
  vertex_count_ = count;
  return true;
}

void FxVolumeRenderer::draw(RhiDevice& device, RhiCommandList& cmd,
                            const DrawParams& params) {
  vertex_count_ = 0;
  if (!ready() || params.volumes == nullptr || params.volumes->live == 0 ||
      params.depth == RHI_TEXTURE_INVALID || !upload(device, params)) {
    return;
  }
  cmd.bindPipeline(pipeline_);
  cmd.setViewport(params.viewport);
  cmd.setScissor(params.scissor);
  cmd.bindVertexBuffer(buffers_[slot_]);
  cmd.bindFragmentTexture(params.depth, FX_VOLUME_DEPTH_SLOT);
  RhiDrawParams draw{};
  draw.vertex_count = vertex_count_;
  cmd.draw(draw);
}

}  // namespace eng
