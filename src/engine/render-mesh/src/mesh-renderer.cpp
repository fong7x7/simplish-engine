#include "mesh-draw-bindings.h"
#include "mesh-gpu-upload.h"

#include <engine/render-mesh/mesh-fragment-lights.h>
#include <engine/render-mesh/mesh-renderer.h>
#include <engine/render-mesh/mesh-stand-in-texture.h>

namespace eng {

namespace {

  /// Depth texture matching a surface of this size.
  ///
  /// Sampled as well as attached, because the outline pass reads it back
  /// once the scene pass has written it.
  RhiTextureHandle createDepthTexture(RhiDevice& device, uint32_t width,
                                      uint32_t height) {
    RhiTextureDesc desc{};
    desc.width = width;
    desc.height = height;
    desc.format = RhiFormat::D32_FLOAT;
    desc.usage = RhiTextureUsage::DEPTH_STENCIL | RhiTextureUsage::SAMPLED;
    desc.debug_name = "mesh_depth";
    return device.createTexture(desc);
  }

}  // namespace

bool MeshRenderer::createUntexturedStandIn(RhiDevice& device) {
  untextured_ = createMeshStandInTexture(device);
  return untextured_ != RHI_TEXTURE_INVALID;
}

bool MeshRenderer::init(RhiDevice& device) {
  RhiPipelineHandle pipeline = RHI_PIPELINE_INVALID;
  if (!device.tryCreateMeshPipeline(pipeline)) {
    return false;
  }
  pipeline_ = pipeline;
  // A backend with a mesh pipeline but no textures would draw every mesh
  // with whatever was bound last, so the stand-in failing is fatal to the
  // renderer rather than something to carry on without.
  if (!createUntexturedStandIn(device)) {
    device.destroyPipeline(pipeline_);
    pipeline_ = RHI_PIPELINE_INVALID;
    return false;
  }
  return true;
}

void MeshRenderer::shutdown(RhiDevice& device) {
  for (auto& [id, mesh] : meshes_) {
    static_cast<void>(id);
    device.destroyBuffer(mesh.vertices);
    device.destroyBuffer(mesh.indices);
  }
  meshes_.clear();
  destroyDepthTarget(device);
  if (untextured_ != RHI_TEXTURE_INVALID) {
    device.destroyTexture(untextured_);
    untextured_ = RHI_TEXTURE_INVALID;
  }
  if (pipeline_ != RHI_PIPELINE_INVALID) {
    device.destroyPipeline(pipeline_);
    pipeline_ = RHI_PIPELINE_INVALID;
  }
}

void MeshRenderer::destroyDepthTarget(RhiDevice& device) {
  if (depth_target_ != RHI_TEXTURE_INVALID) {
    device.destroyTexture(depth_target_);
    depth_target_ = RHI_TEXTURE_INVALID;
  }
  depth_width_ = 0;
  depth_height_ = 0;
}

namespace {

  /// Create both GPU buffers for a mesh. Either handle is zero on failure.
  MeshRenderer::GpuMeshBuffers uploadMeshBuffers(RhiDevice& device,
                                                 const MeshData& mesh) {
    const auto vertex_bytes =
        static_cast<uint64_t>(mesh.vertices.size() * sizeof(MeshVertex));
    const auto index_bytes =
        static_cast<uint64_t>(mesh.indices.size() * sizeof(uint32_t));
    return {uploadMeshBuffer(device, mesh.vertices.data(), vertex_bytes,
                             RhiBufferUsage::VERTEX),
            uploadMeshBuffer(device, mesh.indices.data(), index_bytes,
                             RhiBufferUsage::INDEX),
            static_cast<uint32_t>(mesh.indices.size())};
  }

}  // namespace

std::optional<MeshGpuId> MeshRenderer::upload(RhiDevice& device,
                                              const MeshData& mesh) {
  if (mesh.vertices.empty() || mesh.indices.empty()) {
    return std::nullopt;
  }
  const GpuMeshBuffers gpu = uploadMeshBuffers(device, mesh);
  if (gpu.vertices == 0 || gpu.indices == 0) {
    device.destroyBuffer(gpu.vertices);
    device.destroyBuffer(gpu.indices);
    return std::nullopt;
  }
  const MeshGpuId id = next_id_++;
  meshes_.emplace(id, gpu);
  return id;
}

RhiTextureHandle MeshRenderer::depthTarget(RhiDevice& device, uint32_t width,
                                           uint32_t height) {
  if (!ready() || width == 0 || height == 0) {
    return RHI_TEXTURE_INVALID;
  }
  if (depth_target_ != RHI_TEXTURE_INVALID && depth_width_ == width &&
      depth_height_ == height) {
    return depth_target_;
  }
  destroyDepthTarget(device);
  depth_target_ = createDepthTexture(device, width, height);
  depth_width_ = width;
  depth_height_ = height;
  return depth_target_;
}

void MeshRenderer::bindLights(RhiCommandList& cmd,
                              const DrawParams& params) const {
  const MeshFragmentLights block =
      makeMeshFragmentLights(params.lights, params.shade_bands);
  cmd.setFragmentStageBytes(&block, sizeof(block), MESH_LIGHT_SLOT);
}

void MeshRenderer::drawInstance(RhiCommandList& cmd,
                                const MeshInstance& instance,
                                const Mat4& view_projection) const {
  auto it = meshes_.find(instance.mesh);
  if (it == meshes_.end()) {
    return;
  }
  const MeshVertexUniforms uniforms{view_projection, instance.model};
  cmd.setVertexStageBytes(&uniforms, sizeof(uniforms), MESH_UNIFORM_SLOT);
  // An instance with no map of its own takes the stand-in, so the slot is
  // never left holding the previous instance's texture.
  cmd.bindFragmentTexture(
      instance.texture != RHI_TEXTURE_INVALID ? instance.texture : untextured_,
      MESH_TEXTURE_SLOT);
  cmd.bindVertexBuffer(it->second.vertices);
  cmd.bindIndexBuffer(it->second.indices, 0, RhiIndexType::UINT32);
  RhiDrawIndexedParams params{};
  params.index_count = it->second.index_count;
  cmd.drawIndexed(params);
}

void MeshRenderer::draw(RhiCommandList& cmd, const DrawParams& params) const {
  if (!ready() || params.instances.empty()) {
    return;
  }
  cmd.bindPipeline(pipeline_);
  cmd.setViewport(params.viewport);
  cmd.setScissor(params.scissor);
  bindLights(cmd, params);
  for (const auto& instance : params.instances) {
    drawInstance(cmd, instance, params.view_projection);
  }
}

}  // namespace eng
