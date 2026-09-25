#include <cmath>
#include <cstring>
#include <engine/render-mesh/mesh-fragment-lights.h>
#include <engine/render-water/water-renderer.h>
#include <engine/render-water/water-surface-mesh.h>
#include <engine/render-water/water-texels.h>
#include <engine/render-water/water-vertex-uniforms.h>
#include <engine/render/rhi-buffer-desc.h>
#include <engine/render/rhi-draw-indexed-params.h>
#include <engine/render/rhi-texture-desc.h>
#include <engine/render/rhi-texture-update-2d.h>

namespace eng {

namespace {

  /// Vertex stage slot the matrix and the field's placement are read from.
  constexpr uint32_t WATER_UNIFORM_SLOT = 1;
  /// Fragment stage slot the `WaterShading` block is read from.
  constexpr uint32_t WATER_SHADING_SLOT = 0;
  /// Fragment stage slot the scene's lights are read from, in the mesh
  /// shader's layout.
  constexpr uint32_t WATER_LIGHT_SLOT = 1;
  /// Fragment stage slot the field texture is bound at.
  constexpr uint32_t WATER_FIELD_SLOT = 0;
  /// Fragment stage slot the copy of the scene is bound at.
  constexpr uint32_t WATER_SCENE_SLOT = 1;
  /// Fragment stage slot the scene's depth is bound at.
  constexpr uint32_t WATER_DEPTH_SLOT = 2;
  /// Fragment stage slot what does not move about the field is bound at.
  constexpr uint32_t WATER_STILL_SLOT = 3;

  /// How bright the lights' glints off the water are.
  constexpr float WATER_GLINT = 1.4f;

  /// The seconds the wind waves wrap at, so a long session does not
  /// lose the precision their phases need.
  constexpr float WATER_WAVE_PERIOD = 1024.0f;

  /// A host-visible buffer of @p bytes, filled from @p data; 0 when either
  /// step fails, with nothing left allocated.
  RhiBufferHandle uploadBuffer(RhiDevice& device, const void* data,
                               uint64_t bytes, RhiBufferUsage usage) {
    const RhiBufferHandle buffer = device.createBuffer(
        {bytes, usage, true,
         usage == RhiBufferUsage::INDEX ? "water_surface_indices"
                                        : "water_surface_vertices"});
    void* mapped = buffer != 0 ? device.mapBuffer(buffer) : nullptr;
    if (mapped == nullptr) {
      if (buffer != 0) {
        device.destroyBuffer(buffer);
      }
      return 0;
    }
    std::memcpy(mapped, data, bytes);
    device.unmapBuffer(buffer);
    return buffer;
  }

  /// A field texture of @p width × @p height, starting as @p still.
  ///
  /// The texels are what makes the texture one the CPU can write again:
  /// a backend keeps a texture created without any where only the GPU
  /// reaches it.
  RhiTextureHandle createFieldTexture(RhiDevice& device,
                                      const WaterField& field,
                                      const std::vector<uint8_t>& still) {
    RhiTextureDesc desc{};
    desc.width = field.width;
    desc.height = field.height;
    desc.format = RhiFormat::RGB_A8_UNORM;
    desc.usage = RhiTextureUsage::SAMPLED;
    desc.debug_name = "water_field";
    desc.initial_pixels = still.data();
    return device.createTexture(desc);
  }

  /// A texture to copy the scene into: as big as @p copy's source and in
  /// its format.
  RhiTextureHandle createSceneTexture(RhiDevice& device,
                                      const WaterSceneCopy& copy) {
    RhiTextureDesc desc{};
    desc.width = copy.width;
    desc.height = copy.height;
    desc.format = copy.format;
    desc.usage = RhiTextureUsage::SAMPLED | RhiTextureUsage::TRANSFER_DST;
    desc.debug_name = "water_scene";
    return device.createTexture(desc);
  }

  /// @p copy with its format settled: sRGB RGBA8 when it names none.
  WaterSceneCopy settled(WaterSceneCopy copy) {
    if (copy.format == RhiFormat::UNDEFINED) {
      copy.format = RhiFormat::RGB_A8_SRGB;
    }
    return copy;
  }

  /// Whether @p a and @p b need the same copy of the scene.
  bool sameShape(const WaterSceneCopy& a, const WaterSceneCopy& b) {
    return a.width == b.width && a.height == b.height && a.format == b.format;
  }

  /// A still-water update of @p texels, @p field's size.
  RhiTextureUpdate2D fieldUpdate(const WaterField& field,
                                 const std::vector<uint8_t>& texels) {
    RhiTextureUpdate2D update{};
    update.pixels = texels.data();
    update.width = field.width;
    update.height = field.height;
    update.format = RhiFormat::RGB_A8_UNORM;
    return update;
  }

  /// @p v scaled to unit length, or +Z when it has none.
  Vec3 unitOr(const Vec3& v) {
    const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return length > 1e-6f ? Vec3{v.x / length, v.y / length, v.z / length}
                          : Vec3{0.0f, 0.0f, 1.0f};
  }

  /// The way towards the eye, in world space. The camera is orthographic,
  /// so it is the same for every pixel: against the depth row of the
  /// matrix, since depth grows away from the eye.
  Vec3 towardEye(const Mat4& view_projection) {
    return unitOr({-view_projection(2, 0), -view_projection(2, 1),
                   -view_projection(2, 2)});
  }

  /// @p rgb and @p w into @p out.
  void put(float* out, const Vec3& rgb, float w) {
    out[0] = rgb.x;
    out[1] = rgb.y;
    out[2] = rgb.z;
    out[3] = w;
  }

  /// Where @p field lies, into @p out: its corner, and one over its size,
  /// in tiles.
  void placeField(const WaterField& field, float* out) {
    const auto tiles = static_cast<float>(field.samples_per_tile);
    out[0] = static_cast<float>(field.origin.x);
    out[1] = static_cast<float>(field.origin.y);
    out[2] = tiles / static_cast<float>(field.width);
    out[3] = tiles / static_cast<float>(field.height);
  }

  /// How far a reflection is looked for at @p fidelity: as far as
  /// @p look says at High, and two thirds of that below it.
  float reflectionReach(const WaterLook& look, WaterFidelity fidelity) {
    return fidelity == WaterFidelity::HIGH ? look.reflection_reach
                                           : look.reflection_reach * 0.66f;
  }

  /// @p look into @p shading, as much of it as @p fidelity draws.
  void putLook(WaterShading& shading, const WaterLook& look,
               WaterFidelity fidelity) {
    const bool high = fidelity == WaterFidelity::HIGH;
    put(shading.sky, look.sky, look.reflection);
    put(shading.foam, look.foam, high ? 1.0f : 0.0f);
    put(shading.clarity,
        {look.clear_absorption, look.murky_absorption, look.colour_depth},
        look.deep_darkening);
    put(shading.absorb, look.absorption_tint, look.refraction);
    const float lapping =
        waterFidelitySimulates(fidelity) ? look.lapping : 0.0f;
    put(shading.light, {look.wet_darkening, lapping, look.choppiness},
        WATER_GLINT);
    shading.surface[3] = reflectionReach(look, fidelity);
  }

  /// How much detail @p fidelity draws, into @p shading.
  void putDetail(WaterShading& shading, WaterFidelity fidelity) {
    const bool high = fidelity == WaterFidelity::HIGH;
    const bool moving = waterFidelitySimulates(fidelity);
    const float caustics = high ? 1.0f : moving ? 0.4f : 0.0f;
    // A still surface does not run, however the water flows.
    put(shading.detail, {high ? 1.0f : 0.0f, caustics, moving ? 1.0f : 0.0f},
        moving ? 1.0f : 0.0f);
  }

  /// How many tiles one pixel of @p viewport covers on the ground under
  /// @p view_projection, along whichever of the screen's axes is coarser.
  /// The camera is orthographic, so it is the same everywhere.
  float tilesPerPixel(const Mat4& view_projection,
                      const RhiViewport& viewport) {
    const float across =
        std::hypot(view_projection(0, 0), view_projection(0, 1)) * 0.5f *
        viewport.width;
    const float up = std::hypot(view_projection(1, 0), view_projection(1, 1)) *
                     0.5f * viewport.height;
    const float pixels = std::min(across, up);
    return pixels > 0.0f ? 1.0f / pixels : 0.0f;
  }

  /// Leave out of @p shading whatever @p effects switches off: a strength
  /// of nothing is what tells the shader to skip it.
  void switchEffects(WaterShading& shading, const WaterEffects& effects) {
    if (!waterEffectOn(effects, WaterEffect::REFLECTIONS)) {
      shading.sky[3] = 0.0f;
      shading.surface[3] = 0.0f;
    }
    if (!waterEffectOn(effects, WaterEffect::REFRACTION)) {
      shading.absorb[3] = 0.0f;
    }
    if (!waterEffectOn(effects, WaterEffect::CAUSTICS)) {
      shading.detail[1] = 0.0f;
    }
    shading.toggles[0] =
        waterEffectOn(effects, WaterEffect::CONTACT) ? 1.0f : 0.0f;
  }

  /// One over @p v, or 0 for nothing.
  float inverse(float v) {
    return v > 0.0f ? 1.0f / v : 0.0f;
  }

  /// The vertex block for a draw with @p view_projection over a field
  /// placed at @p field.
  WaterVertexUniforms vertexUniforms(const Mat4& view_projection,
                                     const float* field) {
    WaterVertexUniforms uniforms{view_projection, {}};
    std::memcpy(uniforms.field, field, sizeof(uniforms.field));
    return uniforms;
  }

}  // namespace

bool WaterRenderer::init(RhiDevice& device) {
  RhiPipelineHandle pipeline = RHI_PIPELINE_INVALID;
  if (!device.tryCreateWaterPipeline(pipeline)) {
    return false;
  }
  pipeline_ = pipeline;
  return true;
}

void WaterRenderer::shutdown(RhiDevice& device) {
  releaseSurface(device);
  releaseField(device);
  releaseScene(device);
  if (pipeline_ != RHI_PIPELINE_INVALID) {
    device.destroyPipeline(pipeline_);
    pipeline_ = RHI_PIPELINE_INVALID;
  }
}

void WaterRenderer::releaseSurface(RhiDevice& device) {
  if (vertices_ != 0) {
    device.destroyBuffer(vertices_);
  }
  if (indices_ != 0) {
    device.destroyBuffer(indices_);
  }
  vertices_ = 0;
  indices_ = 0;
  index_count_ = 0;
}

void WaterRenderer::releaseField(RhiDevice& device) {
  for (RhiTextureHandle& texture : textures_) {
    if (texture != RHI_TEXTURE_INVALID) {
      device.destroyTexture(texture);
      texture = RHI_TEXTURE_INVALID;
    }
  }
  if (still_ != RHI_TEXTURE_INVALID) {
    device.destroyTexture(still_);
    still_ = RHI_TEXTURE_INVALID;
  }
  texture_width_ = 0;
  texture_height_ = 0;
}

void WaterRenderer::releaseScene(RhiDevice& device) {
  if (scene_ != RHI_TEXTURE_INVALID) {
    device.destroyTexture(scene_);
    scene_ = RHI_TEXTURE_INVALID;
  }
  scene_shape_ = {};
}

bool WaterRenderer::setSurface(RhiDevice& device, const MeshData& mesh) {
  releaseSurface(device);
  if (!ready() || mesh.indices.empty()) {
    return ready();
  }
  vertices_ = uploadBuffer(device, mesh.vertices.data(),
                           mesh.vertices.size() * sizeof(MeshVertex),
                           RhiBufferUsage::VERTEX);
  indices_ = uploadBuffer(device, mesh.indices.data(),
                          mesh.indices.size() * sizeof(uint32_t),
                          RhiBufferUsage::INDEX);
  if (vertices_ == 0 || indices_ == 0) {
    releaseSurface(device);
    return false;
  }
  index_count_ = static_cast<uint32_t>(mesh.indices.size());
  return true;
}

bool WaterRenderer::sizeField(RhiDevice& device, const WaterField& field) {
  if (texture_width_ == field.width && texture_height_ == field.height &&
      still_ != RHI_TEXTURE_INVALID) {
    return true;
  }
  releaseField(device);
  if (!createFieldTextures(device, field)) {
    releaseField(device);
    return false;
  }
  texture_width_ = field.width;
  texture_height_ = field.height;
  return true;
}

bool WaterRenderer::createFieldTextures(RhiDevice& device,
                                        const WaterField& field) {
  writeWaterStillTexels(field, texels_);
  still_ = createFieldTexture(device, field, texels_);
  // Still water, level and unsloped, and no foam.
  texels_.assign(texels_.size(), 128);
  for (RhiTextureHandle& texture : textures_) {
    texture = createFieldTexture(device, field, texels_);
  }
  return still_ != RHI_TEXTURE_INVALID &&
         textures_.back() != RHI_TEXTURE_INVALID;
}

bool WaterRenderer::setField(RhiDevice& device, const WaterField& field) {
  if (!ready() || waterFieldEmpty(field)) {
    releaseField(device);
    return ready();
  }
  if (!sizeField(device, field)) {
    return false;
  }
  writeWaterTexels(field, texels_);
  slot_ = (slot_ + 1) % WATER_FIELD_TEXTURE_COUNT;
  placeField(field, field_);
  texel_[0] = inverse(static_cast<float>(field.width));
  texel_[1] = inverse(static_cast<float>(field.height));
  texel_[2] = inverse(static_cast<float>(field.samples_per_tile));
  return device.updateTexture2D(textures_[slot_], fieldUpdate(field, texels_));
}

bool WaterRenderer::setShape(RhiDevice& device, const WaterField& field) {
  if (!ready() || waterFieldEmpty(field) || !sizeField(device, field)) {
    return false;
  }
  writeWaterStillTexels(field, texels_);
  return device.updateTexture2D(still_, fieldUpdate(field, texels_));
}

bool WaterRenderer::captureScene(RhiDevice& device, RhiCommandList& cmd,
                                 const WaterSceneCopy& copy) {
  const WaterSceneCopy want = settled(copy);
  if (!drawable() || want.source == RHI_TEXTURE_INVALID || want.width == 0 ||
      want.height == 0) {
    return false;
  }
  if (!sameShape(want, scene_shape_) || scene_ == RHI_TEXTURE_INVALID) {
    releaseScene(device);
    scene_ = createSceneTexture(device, want);
  }
  scene_shape_ = want;
  if (scene_ != RHI_TEXTURE_INVALID) {
    cmd.copyTexture(want.source, scene_);
  }
  return scene_ != RHI_TEXTURE_INVALID;
}

bool WaterRenderer::drawable() const {
  return ready() && index_count_ > 0 &&
         textures_[slot_] != RHI_TEXTURE_INVALID &&
         still_ != RHI_TEXTURE_INVALID;
}

WaterShading WaterRenderer::shadingFor(const DrawParams& params) const {
  WaterShading shading{};
  shading.view_projection = params.view_projection;
  putLook(shading, params.look, params.fidelity);
  putDetail(shading, params.fidelity);
  switchEffects(shading, params.effects);
  put(shading.view, towardEye(params.view_projection),
      std::fmod(std::max(params.seconds, 0.0f), WATER_WAVE_PERIOD));
  placeScreen(shading, params.viewport);
  return shading;
}

void WaterRenderer::placeScreen(WaterShading& shading,
                                const RhiViewport& viewport) const {
  shading.screen[0] = viewport.x;
  shading.screen[1] = viewport.y;
  shading.screen[2] = inverse(static_cast<float>(scene_shape_.width));
  shading.screen[3] = inverse(static_cast<float>(scene_shape_.height));
  shading.surface[0] = viewport.width;
  shading.surface[1] = viewport.height;
  shading.surface[2] = WATER_SURFACE_HEIGHT;
  shading.texel[0] = texel_[0];
  shading.texel[1] = texel_[1];
  shading.texel[2] = texel_[2];
  shading.texel[3] = tilesPerPixel(shading.view_projection, viewport);
}

void WaterRenderer::pushBlocks(RhiCommandList& cmd,
                               const DrawParams& params) const {
  const WaterVertexUniforms uniforms =
      vertexUniforms(params.view_projection, field_);
  const WaterShading shading = shadingFor(params);
  const MeshFragmentLights lights =
      makeMeshFragmentLights(params.lights, params.shade_bands);
  cmd.setVertexStageBytes(&uniforms, sizeof(uniforms), WATER_UNIFORM_SLOT);
  cmd.setFragmentStageBytes(&shading, sizeof(shading), WATER_SHADING_SLOT);
  cmd.setFragmentStageBytes(&lights, sizeof(lights), WATER_LIGHT_SLOT);
}

void WaterRenderer::bindTextures(RhiCommandList& cmd,
                                 RhiTextureHandle depth) const {
  cmd.bindFragmentTexture(textures_[slot_], WATER_FIELD_SLOT);
  cmd.bindFragmentTexture(scene_, WATER_SCENE_SLOT);
  cmd.bindFragmentTexture(depth, WATER_DEPTH_SLOT);
  cmd.bindFragmentTexture(still_, WATER_STILL_SLOT);
}

void WaterRenderer::draw(RhiCommandList& cmd, const DrawParams& params) const {
  if (!drawable() || scene_ == RHI_TEXTURE_INVALID ||
      params.depth == RHI_TEXTURE_INVALID) {
    return;
  }
  cmd.bindPipeline(pipeline_);
  cmd.setViewport(params.viewport);
  cmd.setScissor(params.scissor);
  pushBlocks(cmd, params);
  bindTextures(cmd, params.depth);
  cmd.bindVertexBuffer(vertices_);
  cmd.bindIndexBuffer(indices_, 0, RhiIndexType::UINT32);
  cmd.drawIndexed({.index_count = index_count_});
}

}  // namespace eng
