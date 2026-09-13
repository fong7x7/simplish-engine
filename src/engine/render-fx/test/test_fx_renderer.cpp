#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <engine/render-fx/fx-quads.h>
#include <engine/render-fx/fx-renderer.h>
#include <map>
#include <vector>

using Catch::Approx;
using namespace eng;

namespace {

/// The handle the fake backend gives its effects pipeline.
constexpr RhiPipelineHandle FAKE_FX_PIPELINE = 42;
/// A depth texture handle the tests hand the renderer.
constexpr RhiTextureHandle FAKE_DEPTH = 7;

/// Which backend the fake stands in for: one with an effects pipeline, or
/// one without, which is the stub.
enum class FakeFx : uint8_t { PRESENT, ABSENT };

/// Just enough of a device to hand out a pipeline and host-visible
/// buffers, and to take them back.
class FakeDevice final : public RhiDevice {
public:
  explicit FakeDevice(FakeFx fx) : fx_(fx) {}

  bool tryCreateFxParticlePipeline(RhiPipelineHandle& out) override {
    if (fx_ == FakeFx::ABSENT) {
      return false;
    }
    out = FAKE_FX_PIPELINE;
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

  /// Every live buffer's bytes, by handle.
  std::map<RhiBufferHandle, std::vector<uint8_t>> memory{};
  /// Every pipeline handed back.
  std::vector<RhiPipelineHandle> destroyed_pipelines{};

  RhiBackend backend() const override { return RhiBackend::STUB; }
  const RhiDeviceCapabilities& capabilities() const override { return caps_; }
  RhiTextureHandle createTexture(const RhiTextureDesc& /*desc*/) override {
    return 0;
  }
  void destroyTexture(RhiTextureHandle /*handle*/) override {}
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
  /// Whether this backend has an effects pipeline to give.
  FakeFx fx_;
  /// Next buffer handle to hand out.
  RhiBufferHandle next_buffer_ = 100;
  /// Capabilities nobody here reads.
  RhiDeviceCapabilities caps_{};
};

/// A command list that remembers what the effects pass asked of it.
class RecordingCommandList final : public RhiCommandList {
public:
  void bindPipeline(RhiPipelineHandle pipeline) override {
    bound_pipeline = pipeline;
  }
  void bindVertexBuffer(RhiBufferHandle buffer, uint64_t /*offset*/) override {
    vertex_buffers.push_back(buffer);
  }
  void setFragmentStageBytes(const void* data, size_t size,
                             uint32_t slot) override {
    fragment_slot = slot;
    fragment_bytes.resize(size);
    std::memcpy(fragment_bytes.data(), data, size);
  }
  void bindFragmentTexture(RhiTextureHandle texture, uint32_t slot) override {
    texture_bound = texture;
    texture_slot = slot;
  }
  void draw(const RhiDrawParams& params) override { draws.push_back(params); }

  /// Last pipeline bound.
  RhiPipelineHandle bound_pipeline = RHI_PIPELINE_INVALID;
  /// Every vertex buffer bound, in order.
  std::vector<RhiBufferHandle> vertex_buffers{};
  /// Last fragment stage payload.
  std::vector<uint8_t> fragment_bytes{};
  /// Slot that payload went to.
  uint32_t fragment_slot = 99;
  /// Last texture bound to the fragment stage.
  RhiTextureHandle texture_bound = RHI_TEXTURE_INVALID;
  /// Slot that texture went to.
  uint32_t texture_slot = 99;
  /// Every non-indexed draw recorded.
  std::vector<RhiDrawParams> draws{};

  void begin() override {}
  void end() override {}
  void beginRenderPass(const RhiRenderPassBeginInfo& /*info*/) override {}
  void endRenderPass() override {}
  void bindIndexBuffer(RhiBufferHandle /*buffer*/, uint64_t /*offset*/,
                       RhiIndexType /*type*/) override {}
  void bindDescriptorSet(uint32_t /*index*/,
                         RhiDescriptorSetHandle /*set*/) override {}
  void setViewport(const RhiViewport& /*viewport*/) override {}
  void setScissor(const RhiScissor& /*scissor*/) override {}
  void drawIndexed(const RhiDrawIndexedParams& /*params*/) override {}
  void dispatch(uint32_t /*x*/, uint32_t /*y*/, uint32_t /*z*/) override {}
  void copyBuffer(const RhiCopyBufferParams& /*params*/) override {}
  void copyTextureToBuffer(RhiTextureHandle /*src*/,
                           RhiBufferHandle /*dst*/) override {}
  void textureBarrier(RhiTextureHandle /*texture*/, RhiTextureLayout /*from*/,
                      RhiTextureLayout /*to*/) override {}
};

/// A pool of @p count round particles, each 0.1 tiles, strung out along x.
FxParticlePool testParticles(uint32_t count) {
  FxParticlePool pool(count);
  for (uint32_t i = 0; i < count; ++i) {
    pool.position[i] = {static_cast<float>(i) * 0.1f, 0.0f, 0.0f};
    pool.life[i] = 1.0f;
    pool.scale[i] = 1.0f;
  }
  pool.live = count;
  return pool;
}

/// A draw of @p particles over a 200-pixel surface.
FxRenderer::DrawParams testParams(const FxParticlePool& particles) {
  FxRenderer::DrawParams params{};
  params.particles = &particles;
  params.depth = FAKE_DEPTH;
  params.view_projection = Mat4::identity();
  params.viewport = {0.0f, 0.0f, 200.0f, 200.0f, 0.0f, 1.0f};
  params.scissor = {0, 0, 200, 200};
  return params;
}

}  // namespace

TEST_CASE("a backend with no effects pipeline leaves the renderer inert",
          "[render-fx][renderer]") {
  FakeDevice device(FakeFx::ABSENT);
  FxRenderer renderer;
  CHECK_FALSE(renderer.init(device));
  CHECK_FALSE(renderer.ready());
  CHECK(device.memory.empty());

  const FxParticlePool particles = testParticles(2);
  RecordingCommandList cmd;
  renderer.draw(device, cmd, testParams(particles));
  CHECK(cmd.draws.empty());
}

TEST_CASE("a draw uploads every particle's quad and draws it over the depth",
          "[render-fx][renderer]") {
  FakeDevice device(FakeFx::PRESENT);
  FxRenderer renderer;
  REQUIRE(renderer.init(device, 8));
  CHECK(device.memory.size() == FX_FRAME_BUFFER_COUNT);

  const FxParticlePool particles = testParticles(3);
  RecordingCommandList cmd;
  renderer.draw(device, cmd, testParams(particles));
  REQUIRE(cmd.draws.size() == 1);
  CHECK(cmd.draws[0].vertex_count == 3 * FX_VERTICES_PER_PARTICLE);
  CHECK(renderer.vertexCount() == 3 * FX_VERTICES_PER_PARTICLE);
  CHECK(cmd.bound_pipeline == FAKE_FX_PIPELINE);
  CHECK(cmd.texture_bound == FAKE_DEPTH);
  CHECK(cmd.texture_slot == 0);
  CHECK(cmd.fragment_slot == 0);
  CHECK(cmd.fragment_bytes.size() == 16);
}

TEST_CASE("what a draw uploads is the quads it lays out",
          "[render-fx][renderer]") {
  FakeDevice device(FakeFx::PRESENT);
  FxRenderer renderer;
  REQUIRE(renderer.init(device, 4));
  const FxParticlePool particles = testParticles(2);
  RecordingCommandList cmd;
  renderer.draw(device, cmd, testParams(particles));
  REQUIRE(cmd.vertex_buffers.size() == 1);

  std::vector<std::pair<float, uint32_t>> order;
  std::vector<FxVertex> expected;
  buildFxQuads(particles, {Mat4::identity(), 200.0f, 200.0f}, order, expected);
  const std::vector<uint8_t>& bytes = device.memory[cmd.vertex_buffers[0]];
  CHECK(std::memcmp(bytes.data(), expected.data(),
                    expected.size() * sizeof(FxVertex)) == 0);
}

TEST_CASE("each frame's draw writes the next buffer in turn",
          "[render-fx][renderer]") {
  FakeDevice device(FakeFx::PRESENT);
  FxRenderer renderer;
  REQUIRE(renderer.init(device, 4));
  const FxParticlePool particles = testParticles(1);
  RecordingCommandList cmd;
  for (uint32_t frame = 0; frame <= FX_FRAME_BUFFER_COUNT; ++frame) {
    renderer.draw(device, cmd, testParams(particles));
  }

  REQUIRE(cmd.vertex_buffers.size() == FX_FRAME_BUFFER_COUNT + 1);
  CHECK(cmd.vertex_buffers[0] != cmd.vertex_buffers[1]);
  CHECK(cmd.vertex_buffers[1] != cmd.vertex_buffers[2]);
  CHECK(cmd.vertex_buffers[0] == cmd.vertex_buffers[FX_FRAME_BUFFER_COUNT]);
}

TEST_CASE("more particles than a buffer holds draws the nearest",
          "[render-fx][renderer]") {
  FakeDevice device(FakeFx::PRESENT);
  FxRenderer renderer;
  REQUIRE(renderer.init(device, 2));
  FxParticlePool particles = testParticles(3);
  particles.position[0].z = 0.9f;
  RecordingCommandList cmd;
  renderer.draw(device, cmd, testParams(particles));

  REQUIRE(cmd.draws.size() == 1);
  CHECK(cmd.draws[0].vertex_count == 2 * FX_VERTICES_PER_PARTICLE);
  FxVertex first{};
  std::memcpy(&first, device.memory[cmd.vertex_buffers[0]].data(),
              sizeof(first));
  CHECK(first.clip[2] == 0.0f);
}

TEST_CASE("nothing is drawn with no particles or no depth",
          "[render-fx][renderer]") {
  FakeDevice device(FakeFx::PRESENT);
  FxRenderer renderer;
  REQUIRE(renderer.init(device, 4));
  RecordingCommandList cmd;

  const FxParticlePool none = testParticles(0);
  renderer.draw(device, cmd, testParams(none));
  const FxParticlePool some = testParticles(1);
  FxRenderer::DrawParams no_depth = testParams(some);
  no_depth.depth = RHI_TEXTURE_INVALID;
  renderer.draw(device, cmd, no_depth);
  CHECK(cmd.draws.empty());
  CHECK(renderer.vertexCount() == 0);
}

TEST_CASE("softness fades a particle over FX_SOFT_TILES of depth",
          "[render-fx][renderer]") {
  Mat4 m = Mat4::identity();
  m(2, 0) = 0.0f;
  m(2, 1) = 0.006f;
  m(2, 2) = 0.008f;
  CHECK(FxRenderer::softness(m) == Approx(1.0f / (FX_SOFT_TILES * 0.01f)));
  m(2, 1) = 0.0f;
  m(2, 2) = 0.0f;
  CHECK(FxRenderer::softness(m) == 0.0f);
}

TEST_CASE("shutting down gives back the pipeline and every buffer",
          "[render-fx][renderer]") {
  FakeDevice device(FakeFx::PRESENT);
  FxRenderer renderer;
  REQUIRE(renderer.init(device, 4));
  renderer.shutdown(device);

  CHECK_FALSE(renderer.ready());
  CHECK(device.memory.empty());
  REQUIRE(device.destroyed_pipelines.size() == 1);
  CHECK(device.destroyed_pipelines[0] == FAKE_FX_PIPELINE);
}
