#include "mesh-draw-bindings.h"
#include "mesh-gpu-upload.h"

#include <engine/render-mesh/mesh-fragment-lights.h>
#include <engine/render-mesh/mesh-stand-in-texture.h>
#include <engine/render-mesh/skin-palette.h>
#include <engine/render-mesh/skinned-mesh-renderer.h>

namespace eng {

bool SkinnedMeshRenderer::init(RhiDevice& device) {
  RhiPipelineHandle pipeline = RHI_PIPELINE_INVALID;
  if (!device.tryCreateSkinnedMeshPipeline(pipeline)) {
    return false;
  }
  untextured_ = createMeshStandInTexture(device);
  // As for static meshes: a pipeline with nothing to sample for an
  // untextured instance would draw it with whatever was bound last.
  if (untextured_ == RHI_TEXTURE_INVALID) {
    device.destroyPipeline(pipeline);
    return false;
  }
  pipeline_ = pipeline;
  return true;
}

void SkinnedMeshRenderer::shutdown(RhiDevice& device) {
  for (auto& [id, mesh] : meshes_) {
    static_cast<void>(id);
    device.destroyBuffer(mesh.vertices);
    device.destroyBuffer(mesh.indices);
  }
  meshes_.clear();
  device.destroyTexture(untextured_);
  untextured_ = RHI_TEXTURE_INVALID;
  device.destroyPipeline(pipeline_);
  pipeline_ = RHI_PIPELINE_INVALID;
}

SkinnedMeshRenderer::GpuBuffers
SkinnedMeshRenderer::uploadBuffers(RhiDevice& device,
                                   const SkinnedMeshData& mesh) {
  return {uploadMeshBuffer(device, mesh.vertices.data(),
                           mesh.vertices.size() * sizeof(SkinnedMeshVertex),
                           RhiBufferUsage::VERTEX),
          uploadMeshBuffer(device, mesh.indices.data(),
                           mesh.indices.size() * sizeof(uint32_t),
                           RhiBufferUsage::INDEX),
          static_cast<uint32_t>(mesh.indices.size())};
}

std::optional<MeshGpuId>
SkinnedMeshRenderer::upload(RhiDevice& device, const SkinnedMeshData& mesh) {
  if (mesh.vertices.empty() || mesh.indices.empty()) {
    return std::nullopt;
  }
  const GpuBuffers gpu = uploadBuffers(device, mesh);
  if (gpu.vertices == 0 || gpu.indices == 0) {
    device.destroyBuffer(gpu.vertices);
    device.destroyBuffer(gpu.indices);
    return std::nullopt;
  }
  const MeshGpuId id = next_id_++;
  meshes_.emplace(id, gpu);
  return id;
}

void SkinnedMeshRenderer::drawInstance(RhiCommandList& cmd,
                                       const SkinnedMeshInstance& instance,
                                       const Mat4& view_projection) const {
  auto it = meshes_.find(instance.mesh);
  if (it == meshes_.end()) {
    return;
  }
  const MeshVertexUniforms uniforms{view_projection, instance.model};
  cmd.setVertexStageBytes(&uniforms, sizeof(uniforms), MESH_UNIFORM_SLOT);
  const SkinPalette palette = makeSkinPalette(instance.skin);
  cmd.setVertexStageBytes(&palette, sizeof(palette), MESH_SKIN_PALETTE_SLOT);
  cmd.bindFragmentTexture(
      instance.texture != RHI_TEXTURE_INVALID ? instance.texture : untextured_,
      MESH_TEXTURE_SLOT);
  cmd.bindVertexBuffer(it->second.vertices);
  cmd.bindIndexBuffer(it->second.indices, 0, RhiIndexType::UINT32);
  RhiDrawIndexedParams params{};
  params.index_count = it->second.index_count;
  cmd.drawIndexed(params);
}

void SkinnedMeshRenderer::draw(RhiCommandList& cmd,
                               const DrawParams& params) const {
  if (!ready() || params.instances.empty()) {
    return;
  }
  cmd.bindPipeline(pipeline_);
  cmd.setViewport(params.viewport);
  cmd.setScissor(params.scissor);
  const MeshFragmentLights lights =
      makeMeshFragmentLights(params.lights, params.shade_bands);
  cmd.setFragmentStageBytes(&lights, sizeof(lights), MESH_LIGHT_SLOT);
  for (const auto& instance : params.instances) {
    drawInstance(cmd, instance, params.view_projection);
  }
}

}  // namespace eng
