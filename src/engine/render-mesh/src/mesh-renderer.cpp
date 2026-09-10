#include <algorithm>
#include <cstddef>
#include <cstring>
#include <engine/render-mesh/mesh-renderer.h>

namespace eng {

namespace {

  /// Vertex-stage slot the mesh shader reads its matrices from.
  constexpr uint32_t MESH_UNIFORM_SLOT = 1;

  /// Fragment-stage slot the mesh shader reads its lights from.
  constexpr uint32_t MESH_LIGHT_SLOT = 0;

  /// Fragment-stage slot the mesh shader samples its diffuse map from.
  constexpr uint32_t MESH_TEXTURE_SLOT = 0;

  /// The colour an untextured mesh is shaded with, as one sRGB texel.
  ///
  /// This is the constant the shader used to carry, moved into a texture so
  /// that textured and untextured meshes take the same path through it: an
  /// instance naming no map samples this instead of branching. Sampling it
  /// gives back what the old constant was to within a 255th, which is why
  /// nothing already on screen changed when textures arrived.
  constexpr uint8_t UNTEXTURED_TEXEL[4] = {189, 194, 204, 255};

  /// What the vertex stage reads.
  ///
  /// Both matrices rather than their product: the fragment stage shades in
  /// world space, so it needs the world position and the world normal, and
  /// the vertex stage cannot recover either from a combined matrix.
  struct VertexUniforms {
    /// World-to-clip, shared by every instance of the draw.
    Mat4 view_projection{};
    /// Object-to-world for this instance.
    Mat4 model{};
  };

  /// What the fragment stage reads: the light count and the band count,
  /// then the lights.
  ///
  /// A fixed array always sent whole, so a draw with no lights needs no
  /// second code path in the shader and no second binding here.
  struct alignas(16) FragmentLights {
    /// How many entries of `lights` are live.
    uint32_t count = 0;
    /// Tones each light is flattened into; `MESH_SHADE_SMOOTH` for none.
    /// It rides in the count's register, which was padding before it.
    uint32_t shade_bands = MESH_SHADE_SMOOTH;
    /// The rest of that register. The array after it is read as `float4`s,
    /// which have to start on a register boundary.
    uint32_t padding[2]{};
    /// The lights, of which the first `count` are live.
    MeshLight lights[MESH_MAX_LIGHTS]{};
  };

  // The shader's own struct puts the array one register in, and reads the
  // band count as the second word of the first; padding that drifts here
  // shifts every light the shader reads.
  static_assert(offsetof(FragmentLights, shade_bands) == 4,
                "the band count is the header's second word");
  static_assert(offsetof(FragmentLights, lights) == 16,
                "the lights follow the header's whole register");

  /// The lights of a draw, in the layout the shader reads.
  ///
  /// An empty list becomes the one default light, which is the built-in key
  /// light — see `mesh-light.h`. Lights past the array's length are
  /// dropped: the shader's loop is a fixed length and cannot grow.
  FragmentLights toFragmentLights(std::span<const MeshLight> lights,
                                  uint32_t shade_bands) {
    FragmentLights block;
    block.shade_bands = shade_bands;
    if (lights.empty()) {
      block.count = 1;
      return block;
    }
    block.count =
        static_cast<uint32_t>(std::min(lights.size(), MESH_MAX_LIGHTS));
    std::memcpy(block.lights, lights.data(), block.count * sizeof(MeshLight));
    return block;
  }

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
  RhiTextureDesc desc{};
  desc.width = 1;
  desc.height = 1;
  // Unorm, not sRGB: the shader converts to linear itself, on the way out
  // and after the lighting, exactly as it did when this colour was a
  // constant in it. A texture the GPU decoded on sample would be converted
  // twice.
  desc.format = RhiFormat::RGB_A8_UNORM;
  desc.usage = RhiTextureUsage::SAMPLED;
  desc.debug_name = "mesh_untextured";
  desc.initial_pixels = UNTEXTURED_TEXEL;
  untextured_ = device.createTexture(desc);
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

void MeshRenderer::bindLights(RhiCommandList& cmd,
                              const DrawParams& params) const {
  const FragmentLights block =
      toFragmentLights(params.lights, params.shade_bands);
  cmd.setFragmentStageBytes(&block, sizeof(block), MESH_LIGHT_SLOT);
}

void MeshRenderer::drawInstance(RhiCommandList& cmd,
                                const MeshInstance& instance,
                                const Mat4& view_projection) const {
  auto it = meshes_.find(instance.mesh);
  if (it == meshes_.end()) {
    return;
  }
  const VertexUniforms uniforms{view_projection, instance.model};
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
