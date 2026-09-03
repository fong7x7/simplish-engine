#include <cstring>
#include <engine/render-mesh/mesh-renderer.h>

namespace eng {

namespace {

  /// Vertex-stage slot the mesh shader reads its matrix from.
  constexpr uint32_t MESH_UNIFORM_SLOT = 1;

  /// Copy CPU memory into an already-created buffer.
  bool fillBuffer(RhiDevice& device, RhiBufferHandle handle, const void* data,
                  uint64_t bytes) {
    void* mapped = device.mapBuffer(handle);
    if (mapped == nullptr) {
      return false;
    }
    std::memcpy(mapped, data, bytes);
    device.unmapBuffer(handle);
    return true;
  }

  /// Create a host-visible buffer and fill it from CPU memory.
  RhiBufferHandle uploadBuffer(RhiDevice& device, const void* data,
                               uint64_t bytes, RhiBufferUsage usage) {
    RhiBufferDesc desc{};
    desc.size = bytes;
    desc.usage = usage;
    desc.host_visible = true;
    desc.debug_name = "mesh_buffer";
    const RhiBufferHandle handle = device.createBuffer(desc);
    if (handle == 0) {
      return 0;
    }
    if (!fillBuffer(device, handle, data, bytes)) {
      device.destroyBuffer(handle);
      return 0;
    }
    return handle;
  }

  /// Depth texture matching a surface of this size.
  RhiTextureHandle createDepthTexture(RhiDevice& device, uint32_t width,
                                      uint32_t height) {
    RhiTextureDesc desc{};
    desc.width = width;
    desc.height = height;
    desc.format = RhiFormat::D32_FLOAT;
    desc.usage = RhiTextureUsage::DEPTH_STENCIL;
    desc.debug_name = "mesh_depth";
    return device.createTexture(desc);
  }

}  // namespace

bool MeshRenderer::init(RhiDevice& device) {
  RhiPipelineHandle pipeline = RHI_PIPELINE_INVALID;
  if (!device.tryCreateMeshPipeline(pipeline)) {
    return false;
  }
  pipeline_ = pipeline;
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
    return {uploadBuffer(device, mesh.vertices.data(), vertex_bytes,
                         RhiBufferUsage::VERTEX),
            uploadBuffer(device, mesh.indices.data(), index_bytes,
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

void MeshRenderer::drawInstance(RhiCommandList& cmd,
                                const MeshInstance& instance,
                                const Mat4& view_projection) const {
  auto it = meshes_.find(instance.mesh);
  if (it == meshes_.end()) {
    return;
  }
  const Mat4 mvp = view_projection * instance.model;
  cmd.setVertexStageBytes(mvp.m, sizeof(mvp.m), MESH_UNIFORM_SLOT);
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
  for (const auto& instance : params.instances) {
    drawInstance(cmd, instance, params.view_projection);
  }
}

}  // namespace eng
