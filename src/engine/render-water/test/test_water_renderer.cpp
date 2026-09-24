#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <engine/render-mesh/mesh-fragment-lights.h>
#include <engine/render-water/water-renderer.h>
#include <engine/render-water/water-surface-mesh.h>
#include <engine/render-water/water-vertex-uniforms.h>
#include <map>
#include <set>
#include <vector>

using Catch::Approx;
using namespace eng;

namespace {

/// The handle the fake backend gives its water pipeline.
constexpr RhiPipelineHandle FAKE_WATER_PIPELINE = 77;

/// Which backend the fake stands in for: one with a water pipeline, or the
/// stub, which has none.
enum class FakeWater : uint8_t { PRESENT, ABSENT };

/// Just enough of a device to hand out a pipeline, buffers and textures,
/// to take them back, and to remember what was written to a texture.
class FakeDevice final : public RhiDevice {
public:
  explicit FakeDevice(FakeWater water) : water_(water) {}

  bool tryCreateWaterPipeline(RhiPipelineHandle& out) override {
    if (water_ == FakeWater::ABSENT) {
      return false;
    }
    out = FAKE_WATER_PIPELINE;
    return true;
  }
  void destroyPipeline(RhiPipelineHandle handle) override {
    destroyed_pipelines.push_back(handle);
  }
  RhiBufferHandle createBuffer(const RhiBufferDesc& desc) override {
    const RhiBufferHandle handle = next_buffer_++;
    memory[handle].resize(desc.size);
    return handle;
  }
  void destroyBuffer(RhiBufferHandle handle) override { memory.erase(handle); }
  void* mapBuffer(RhiBufferHandle handle) override {
    auto it = memory.find(handle);
    return it == memory.end() ? nullptr : it->second.data();
  }
  void unmapBuffer(RhiBufferHandle /*handle*/) override {}
  RhiTextureHandle createTexture(const RhiTextureDesc& desc) override {
    const RhiTextureHandle handle = next_texture_++;
    textures[handle] = {desc.width, desc.height};
    return handle;
  }
  void destroyTexture(RhiTextureHandle handle) override {
    textures.erase(handle);
  }
  bool updateTexture2D(RhiTextureHandle handle,
                       const RhiTextureUpdate2D& update) override {
    written.push_back(handle);
    return textures.contains(handle) &&
           update.format == RhiFormat::RGB_A8_UNORM;
  }

  /// Every live buffer's bytes, by handle.
  std::map<RhiBufferHandle, std::vector<uint8_t>> memory{};
  /// Every live texture's width and height, by handle.
  std::map<RhiTextureHandle, std::pair<uint32_t, uint32_t>> textures{};
  /// Every texture written, in order.
  std::vector<RhiTextureHandle> written{};
  /// Every pipeline handed back.
  std::vector<RhiPipelineHandle> destroyed_pipelines{};

  RhiBackend backend() const override { return RhiBackend::STUB; }
  const RhiDeviceCapabilities& capabilities() const override { return caps_; }
  RhiShaderHandle createShader(const RhiShaderDesc& /*desc*/) override {
    return 0;
  }
  void destroyShader(RhiShaderHandle /*handle*/) override {}
  RhiPipelineHandle
  createGraphicsPipeline(const RhiGraphicsPipelineDesc& /*desc*/) override {
    return 0;
  }
  RhiPipelineHandle
  createComputePipeline(const RhiComputePipelineDesc& /*desc*/) override {
    return 0;
  }
  RhiTextureHandle backbufferTexture() const override { return 0; }
  uint32_t backbufferWidth() const override { return 0; }
  uint32_t backbufferHeight() const override { return 0; }
  bool beginFrame() override { return true; }
  void endFrame() override {}
  void submit(RhiCommandList& /*cmd*/) override {}
  bool present() override { return true; }
  std::unique_ptr<RhiCommandList> createCommandList() override {
    return nullptr;
  }
  std::optional<RhiCaptureResult>
  captureFramebuffer(const RhiCaptureRequest& /*request*/) override {
    return std::nullopt;
  }
  bool captureToFile(const RhiCaptureRequest& /*request*/,
                     std::string_view /*path*/) override {
    return false;
  }
  IRhiRayTracing* rayTracing() override { return nullptr; }
  void waitIdle() override {}

private:
  /// Whether this backend has a water pipeline to give.
  FakeWater water_;
  /// Next buffer handle to hand out.
  RhiBufferHandle next_buffer_ = 100;
  /// Next texture handle to hand out.
  RhiTextureHandle next_texture_ = 500;
  /// Capabilities nobody here reads.
  RhiDeviceCapabilities caps_{};
};

/// A command list that remembers what the water draw asked of it.
class RecordingCommandList final : public RhiCommandList {
public:
  void bindPipeline(RhiPipelineHandle pipeline) override {
    bound_pipeline = pipeline;
  }
  void bindVertexBuffer(RhiBufferHandle buffer, uint64_t /*offset*/) override {
    vertex_buffer = buffer;
  }
  void bindIndexBuffer(RhiBufferHandle buffer, uint64_t /*offset*/,
                       RhiIndexType type) override {
    index_buffer = buffer;
    index_type = type;
  }
  void setVertexStageBytes(const void* data, size_t size,
                           uint32_t slot) override {
    vertex_slot = slot;
    vertex_bytes.resize(size);
    std::memcpy(vertex_bytes.data(), data, size);
  }
  void setFragmentStageBytes(const void* data, size_t size,
                             uint32_t slot) override {
    std::vector<uint8_t>& bytes = fragment_blocks[slot];
    bytes.resize(size);
    std::memcpy(bytes.data(), data, size);
  }
  void bindFragmentTexture(RhiTextureHandle texture, uint32_t slot) override {
    texture_bound = texture;
    texture_slot = slot;
  }
  void drawIndexed(const RhiDrawIndexedParams& params) override {
    draws.push_back(params);
  }

  /// Last pipeline bound.
  RhiPipelineHandle bound_pipeline = RHI_PIPELINE_INVALID;
  /// Last vertex buffer bound.
  RhiBufferHandle vertex_buffer = 0;
  /// Last index buffer bound.
  RhiBufferHandle index_buffer = 0;
  /// Width of its indices.
  RhiIndexType index_type = RhiIndexType::UINT16;
  /// Last vertex stage payload.
  std::vector<uint8_t> vertex_bytes{};
  /// Slot it went to.
  uint32_t vertex_slot = 99;
  /// The last fragment stage payload at each slot.
  std::map<uint32_t, std::vector<uint8_t>> fragment_blocks{};
  /// Last texture bound to the fragment stage.
  RhiTextureHandle texture_bound = RHI_TEXTURE_INVALID;
  /// Slot it went to.
  uint32_t texture_slot = 99;
  /// Every indexed draw recorded.
  std::vector<RhiDrawIndexedParams> draws{};

  void begin() override {}
  void end() override {}
  void beginRenderPass(const RhiRenderPassBeginInfo& /*info*/) override {}
  void endRenderPass() override {}
  void bindDescriptorSet(uint32_t /*index*/,
                         RhiDescriptorSetHandle /*set*/) override {}
  void setViewport(const RhiViewport& /*viewport*/) override {}
  void setScissor(const RhiScissor& /*scissor*/) override {}
  void draw(const RhiDrawParams& /*params*/) override {}
  void dispatch(uint32_t /*x*/, uint32_t /*y*/, uint32_t /*z*/) override {}
  void copyBuffer(const RhiCopyBufferParams& /*params*/) override {}
  void copyTextureToBuffer(RhiTextureHandle /*src*/,
                           RhiBufferHandle /*dst*/) override {}
  void textureBarrier(RhiTextureHandle /*texture*/, RhiTextureLayout /*from*/,
                      RhiTextureLayout /*to*/) override {}
};

/// A pond @p side cells square with its south-west cell at the origin.
WaterLayer pond(int32_t side) {
  WaterLayer layer;
  for (int32_t y = 0; y < side; ++y) {
    for (int32_t x = 0; x < side; ++x) {
      setWaterCell(layer, {x, y}, {.depth = 16});
    }
  }
  return layer;
}

/// A field over @p layer's water at @p samples_per_tile.
WaterField fieldOver(const WaterLayer& layer, uint32_t samples_per_tile) {
  WaterField field;
  resetWaterField(field, layer, samples_per_tile);
  return field;
}

/// A draw at @p fidelity, looking straight down.
WaterRenderer::DrawParams testParams(WaterFidelity fidelity) {
  WaterRenderer::DrawParams params{};
  params.view_projection = Mat4::identity();
  params.viewport = {0.0f, 0.0f, 200.0f, 200.0f, 0.0f, 1.0f};
  params.scissor = {0, 0, 200, 200};
  params.fidelity = fidelity;
  params.seconds = 3.5f;
  return params;
}

/// A renderer on @p device with @p grid's surface and field already handed
/// over.
WaterRenderer readyRenderer(FakeDevice& device, const WaterLayer& grid) {
  WaterRenderer renderer;
  REQUIRE(renderer.init(device));
  REQUIRE(renderer.setSurface(device, makeWaterSurfaceMesh(grid)));
  REQUIRE(renderer.setField(device, fieldOver(grid, 4)));
  return renderer;
}

/// A red point light over the pond.
MeshLight redLamp() {
  MeshLight lamp{};
  lamp.kind = MESH_LIGHT_POINT;
  lamp.position = {1.0f, 1.0f, 2.0f};
  lamp.range = 4.0f;
  lamp.color = {1.0f, 0.2f, 0.1f};
  return lamp;
}

/// The fragment block @p renderer draws with at @p fidelity.
WaterShading shadingAt(const WaterRenderer& renderer, WaterFidelity fidelity) {
  RecordingCommandList cmd;
  renderer.draw(cmd, testParams(fidelity));
  WaterShading shading{};
  REQUIRE(cmd.fragment_blocks[0].size() == sizeof(shading));
  std::memcpy(&shading, cmd.fragment_blocks[0].data(), sizeof(shading));
  return shading;
}

}  // namespace

TEST_CASE("a backend with no water pipeline leaves the water flat",
          "[render-water][renderer]") {
  FakeDevice device(FakeWater::ABSENT);
  WaterRenderer renderer;
  CHECK_FALSE(renderer.init(device));
  CHECK_FALSE(renderer.ready());
  const WaterLayer grid = pond(3);
  CHECK_FALSE(renderer.setSurface(device, makeWaterSurfaceMesh(grid)));
  CHECK_FALSE(renderer.setField(device, fieldOver(grid, 4)));
  CHECK(device.memory.empty());
  CHECK(device.textures.empty());
  RecordingCommandList cmd;
  renderer.draw(cmd, testParams(WaterFidelity::HIGH));
  CHECK(cmd.draws.empty());
}

TEST_CASE("handing over a surface and a field makes their buffers",
          "[render-water][renderer]") {
  FakeDevice device(FakeWater::PRESENT);
  const WaterLayer grid = pond(3);
  WaterRenderer renderer = readyRenderer(device, grid);
  CHECK(renderer.indexCount() == makeWaterSurfaceMesh(grid).indices.size());
  CHECK(device.memory.size() == 2);
  CHECK(device.textures.size() == WATER_FIELD_TEXTURE_COUNT);
  CHECK(renderer.fieldTexture() == device.written.back());
}

TEST_CASE("the surface is drawn once, indexed, over the field it was handed",
          "[render-water][renderer]") {
  FakeDevice device(FakeWater::PRESENT);
  WaterRenderer renderer = readyRenderer(device, pond(3));
  RecordingCommandList cmd;
  renderer.draw(cmd, testParams(WaterFidelity::HIGH));
  REQUIRE(cmd.draws.size() == 1);
  CHECK(cmd.draws[0].index_count == renderer.indexCount());
  CHECK(cmd.bound_pipeline == FAKE_WATER_PIPELINE);
  CHECK(cmd.index_type == RhiIndexType::UINT32);
  CHECK(cmd.texture_bound == renderer.fieldTexture());
  CHECK(cmd.texture_slot == 0);
  CHECK(cmd.vertex_slot == 1);
  CHECK(cmd.vertex_bytes.size() == sizeof(WaterVertexUniforms));
  CHECK(cmd.fragment_blocks[0].size() == sizeof(WaterShading));
  CHECK(cmd.fragment_blocks[1].size() == sizeof(MeshFragmentLights));
}

TEST_CASE("the vertex block places the field where the water is",
          "[render-water][renderer]") {
  FakeDevice device(FakeWater::PRESENT);
  WaterRenderer renderer = readyRenderer(device, pond(3));
  RecordingCommandList cmd;
  renderer.draw(cmd, testParams(WaterFidelity::LOW));
  WaterVertexUniforms uniforms{};
  REQUIRE(cmd.vertex_bytes.size() == sizeof(uniforms));
  std::memcpy(&uniforms, cmd.vertex_bytes.data(), sizeof(uniforms));
  // Three tiles of water and a dry tile either side: five tiles across.
  CHECK(uniforms.field[0] == -1.0f);
  CHECK(uniforms.field[1] == -1.0f);
  CHECK(uniforms.field[2] == Approx(1.0f / 5.0f));
  CHECK(uniforms.field[3] == Approx(1.0f / 5.0f));
}

TEST_CASE("fidelity sets the detail the fragment stage shades with",
          "[render-water][renderer]") {
  FakeDevice device(FakeWater::PRESENT);
  WaterRenderer renderer = readyRenderer(device, pond(3));
  const WaterShading high = shadingAt(renderer, WaterFidelity::HIGH);
  const WaterShading low = shadingAt(renderer, WaterFidelity::LOW);
  CHECK(high.detail[0] == 1.0f);
  CHECK(low.detail[0] == 0.0f);
  CHECK(high.foam[3] > low.foam[3]);
  CHECK(high.view[3] == 3.5f);
}

TEST_CASE("flat water is a still surface: drawn, with no waves",
          "[render-water][renderer]") {
  FakeDevice device(FakeWater::PRESENT);
  WaterRenderer renderer = readyRenderer(device, pond(3));
  RecordingCommandList cmd;
  renderer.draw(cmd, testParams(WaterFidelity::FLAT));
  CHECK(cmd.draws.size() == 1);
  const WaterShading flat = shadingAt(renderer, WaterFidelity::FLAT);
  CHECK(flat.detail[2] == 0.0f);
  CHECK(flat.detail[1] == 0.0f);
  CHECK(shadingAt(renderer, WaterFidelity::LOW).detail[2] == 1.0f);
}

TEST_CASE("the eye reaches the fragment stage as a unit direction",
          "[render-water][renderer]") {
  FakeDevice device(FakeWater::PRESENT);
  WaterRenderer renderer = readyRenderer(device, pond(3));
  const WaterShading shading = shadingAt(renderer, WaterFidelity::HIGH);
  // The identity's depth row runs along +Z, so the eye is below: −Z.
  CHECK(shading.view[2] == -1.0f);
  CHECK(shading.light[3] > 0.0f);
}

TEST_CASE("the water is lit by the lights the meshes are",
          "[render-water][renderer]") {
  FakeDevice device(FakeWater::PRESENT);
  WaterRenderer renderer = readyRenderer(device, pond(3));
  const MeshLight lights[] = {MeshLight{}, redLamp()};
  WaterRenderer::DrawParams params = testParams(WaterFidelity::HIGH);
  params.lights = lights;
  params.shade_bands = 3;
  RecordingCommandList cmd;
  renderer.draw(cmd, params);
  MeshFragmentLights block{};
  REQUIRE(cmd.fragment_blocks[1].size() == sizeof(block));
  std::memcpy(&block, cmd.fragment_blocks[1].data(), sizeof(block));
  CHECK(block.count == 2);
  CHECK(block.shade_bands == 3);
  CHECK(block.lights[1].kind == MESH_LIGHT_POINT);
  CHECK(block.lights[1].color.y == Approx(0.2f));
}

TEST_CASE("a draw given no lights is lit by the key light",
          "[render-water][renderer]") {
  FakeDevice device(FakeWater::PRESENT);
  WaterRenderer renderer = readyRenderer(device, pond(3));
  RecordingCommandList cmd;
  renderer.draw(cmd, testParams(WaterFidelity::LOW));
  MeshFragmentLights block{};
  REQUIRE(cmd.fragment_blocks[1].size() == sizeof(block));
  std::memcpy(&block, cmd.fragment_blocks[1].data(), sizeof(block));
  CHECK(block.count == 1);
}

TEST_CASE("no surface or no field draws nothing", "[render-water][renderer]") {
  FakeDevice device(FakeWater::PRESENT);
  WaterRenderer renderer = readyRenderer(device, pond(3));
  CHECK(renderer.drawable());
  REQUIRE(renderer.setField(device, WaterField{}));
  CHECK(device.textures.empty());
  CHECK_FALSE(renderer.drawable());

  REQUIRE(renderer.setField(device, fieldOver(pond(3), 4)));
  REQUIRE(renderer.setSurface(device, MeshData{}));
  CHECK(device.memory.empty());
  RecordingCommandList bare;
  renderer.draw(bare, testParams(WaterFidelity::HIGH));
  CHECK(bare.draws.empty());
}

TEST_CASE("each frame's field goes into the next texture in turn",
          "[render-water][renderer]") {
  FakeDevice device(FakeWater::PRESENT);
  const WaterLayer grid = pond(3);
  WaterRenderer renderer = readyRenderer(device, grid);
  const WaterField field = fieldOver(grid, 4);
  std::set<RhiTextureHandle> seen{device.written.back()};
  for (uint32_t frame = 1; frame < WATER_FIELD_TEXTURE_COUNT; ++frame) {
    REQUIRE(renderer.setField(device, field));
    seen.insert(device.written.back());
  }
  CHECK(seen.size() == WATER_FIELD_TEXTURE_COUNT);
  REQUIRE(renderer.setField(device, field));
  CHECK(seen.contains(device.written.back()));
  // A field of another size makes the textures anew.
  REQUIRE(renderer.setField(device, fieldOver(pond(5), 4)));
  CHECK_FALSE(seen.contains(device.written.back()));
  CHECK(device.textures.size() == WATER_FIELD_TEXTURE_COUNT);
}

TEST_CASE("shutting down hands everything back", "[render-water][renderer]") {
  FakeDevice device(FakeWater::PRESENT);
  WaterRenderer renderer = readyRenderer(device, pond(3));
  renderer.shutdown(device);
  CHECK_FALSE(renderer.ready());
  CHECK(device.memory.empty());
  CHECK(device.textures.empty());
  CHECK(device.destroyed_pipelines ==
        std::vector<RhiPipelineHandle>{FAKE_WATER_PIPELINE});
}
