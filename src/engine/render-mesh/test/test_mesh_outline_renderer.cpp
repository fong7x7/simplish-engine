#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <engine/render-mesh/mesh-outline-renderer.h>
#include <engine/render-mesh/mesh-style.h>
#include <vector>

using Catch::Approx;
using namespace eng;

namespace {

/// The handle the fake backend gives its outline pipeline.
constexpr RhiPipelineHandle FAKE_OUTLINE_PIPELINE = 42;
/// A depth texture handle the tests hand the renderer.
constexpr RhiTextureHandle FAKE_DEPTH = 7;
/// The surface the tests draw onto, in pixels.
constexpr float SURFACE_W = 800.0f;

/// Which backend the fake stands in for: one with an outline pipeline, or
/// one without, which is Vulkan and the stub today.
enum class FakeOutline : uint8_t { PRESENT, ABSENT };

/// Just enough of a device to hand out, and take back, one pipeline.
class FakeDevice final : public RhiDevice {
public:
  explicit FakeDevice(FakeOutline outline) : outline_(outline) {}

  bool tryCreateMeshOutlinePipeline(RhiPipelineHandle& out) override {
    if (outline_ == FakeOutline::ABSENT) {
      return false;
    }
    out = FAKE_OUTLINE_PIPELINE;
    return true;
  }
  void destroyPipeline(RhiPipelineHandle handle) override {
    destroyed.push_back(handle);
  }

  /// Every pipeline handed back through `destroyPipeline`.
  std::vector<RhiPipelineHandle> destroyed{};

  RhiBackend backend() const override { return RhiBackend::STUB; }
  const RhiDeviceCapabilities& capabilities() const override { return caps_; }
  RhiBufferHandle createBuffer(const RhiBufferDesc& /*desc*/) override {
    return 0;
  }
  void destroyBuffer(RhiBufferHandle /*handle*/) override {}
  void* mapBuffer(RhiBufferHandle /*handle*/) override { return nullptr; }
  void unmapBuffer(RhiBufferHandle /*handle*/) override {}
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
  /// Whether this backend has an outline pipeline to give.
  FakeOutline outline_;
  /// Capabilities nobody here reads.
  RhiDeviceCapabilities caps_{};
};

/// A command list that remembers what the outline pass asked of it.
class RecordingCommandList final : public RhiCommandList {
public:
  void bindPipeline(RhiPipelineHandle pipeline) override {
    bound_pipeline = pipeline;
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
  void setScissor(const RhiScissor& scissor) override { scissor_set = scissor; }
  void draw(const RhiDrawParams& params) override { draws.push_back(params); }

  /// The float at @p index of the fragment payload.
  [[nodiscard]] float fragmentFloat(size_t index) const {
    float value = 0.0f;
    std::memcpy(&value, fragment_bytes.data() + index * sizeof(float),
                sizeof(float));
    return value;
  }

  /// Last pipeline bound.
  RhiPipelineHandle bound_pipeline = RHI_PIPELINE_INVALID;
  /// Last fragment stage payload.
  std::vector<uint8_t> fragment_bytes{};
  /// Slot that payload went to.
  uint32_t fragment_slot = 99;
  /// Last texture bound to the fragment stage.
  RhiTextureHandle texture_bound = RHI_TEXTURE_INVALID;
  /// Slot that texture went to.
  uint32_t texture_slot = 99;
  /// Last scissor set.
  RhiScissor scissor_set{};
  /// Every non-indexed draw recorded.
  std::vector<RhiDrawParams> draws{};

  void begin() override {}
  void end() override {}
  void beginRenderPass(const RhiRenderPassBeginInfo& /*info*/) override {}
  void endRenderPass() override {}
  void bindVertexBuffer(RhiBufferHandle /*buffer*/,
                        uint64_t /*offset*/) override {}
  void bindIndexBuffer(RhiBufferHandle /*buffer*/, uint64_t /*offset*/,
                       RhiIndexType /*type*/) override {}
  void bindDescriptorSet(uint32_t /*index*/,
                         RhiDescriptorSetHandle /*set*/) override {}
  void setViewport(const RhiViewport& /*viewport*/) override {}
  void drawIndexed(const RhiDrawIndexedParams& /*params*/) override {}
  void dispatch(uint32_t /*x*/, uint32_t /*y*/, uint32_t /*z*/) override {}
  void copyBuffer(const RhiCopyBufferParams& /*params*/) override {}
  void copyTextureToBuffer(RhiTextureHandle /*src*/,
                           RhiBufferHandle /*dst*/) override {}
  void textureBarrier(RhiTextureHandle /*texture*/, RhiTextureLayout /*from*/,
                      RhiTextureLayout /*to*/) override {}
};

/// A world-to-clip matrix whose screen rows move @p ndc_per_world across
/// clip space per world unit, and whose depth row moves @p depth_per_world.
Mat4 orthoMatrix(float ndc_per_world, float depth_per_world) {
  Mat4 m = Mat4::identity();
  m(0, 0) = ndc_per_world;
  m(1, 1) = ndc_per_world;
  m(2, 2) = depth_per_world;
  return m;
}

/// An outline pass over the whole test surface, a pixel wide.
MeshOutlineRenderer::DrawParams outlineParams() {
  MeshOutlineRenderer::DrawParams params{};
  params.depth = FAKE_DEPTH;
  params.view_projection = orthoMatrix(0.1f, 0.001f);
  params.width = 1.0f;
  params.color = {0.1f, 0.2f, 0.3f};
  params.viewport = {0.0f, 0.0f, SURFACE_W, 600.0f, 0.0f, 1.0f};
  params.scissor = {10, 20, 300, 200};
  return params;
}

/// Record one outline pass into @p cmd through a backend that has the
/// pipeline.
void recordOutline(RecordingCommandList& cmd,
                   const MeshOutlineRenderer::DrawParams& params) {
  FakeDevice device(FakeOutline::PRESENT);
  MeshOutlineRenderer renderer;
  REQUIRE(renderer.init(device));
  renderer.draw(cmd, params);
}

}  // namespace

TEST_CASE("a backend without an outline pipeline leaves the renderer inert") {
  FakeDevice device(FakeOutline::ABSENT);
  MeshOutlineRenderer renderer;
  REQUIRE_FALSE(renderer.init(device));
  REQUIRE_FALSE(renderer.ready());

  RecordingCommandList cmd;
  renderer.draw(cmd, outlineParams());
  REQUIRE(cmd.draws.empty());
}

TEST_CASE("an outline is one full-screen triangle over the scene's depth") {
  RecordingCommandList cmd;
  recordOutline(cmd, outlineParams());

  REQUIRE(cmd.bound_pipeline == FAKE_OUTLINE_PIPELINE);
  REQUIRE(cmd.texture_bound == FAKE_DEPTH);
  REQUIRE(cmd.texture_slot == 0);
  REQUIRE(cmd.draws.size() == 1);
  REQUIRE(cmd.draws[0].vertex_count == 3);
}

TEST_CASE("the outline's colour and bounds land where the shaders read") {
  // Three float4s: colour, the scissor's edges, then width and threshold.
  // The MSL, HLSL and GLSL all declare this layout by hand.
  RecordingCommandList cmd;
  recordOutline(cmd, outlineParams());

  REQUIRE(cmd.fragment_slot == 0);
  REQUIRE(cmd.fragment_bytes.size() == 48);
  REQUIRE(cmd.fragmentFloat(0) == Approx(0.1f));
  REQUIRE(cmd.fragmentFloat(3) == Approx(1.0f));
  REQUIRE(cmd.fragmentFloat(4) == Approx(10.0f));
  REQUIRE(cmd.fragmentFloat(5) == Approx(20.0f));
  REQUIRE(cmd.fragmentFloat(6) == Approx(310.0f));
  REQUIRE(cmd.fragmentFloat(7) == Approx(220.0f));
}

TEST_CASE("the outline's width and threshold follow its bounds") {
  RecordingCommandList cmd;
  const MeshOutlineRenderer::DrawParams params = outlineParams();
  recordOutline(cmd, params);

  REQUIRE(cmd.fragmentFloat(8) == Approx(1.0f));
  REQUIRE(cmd.fragmentFloat(9) ==
          Approx(MeshOutlineRenderer::creaseThreshold(params.view_projection,
                                                      SURFACE_W, 1.0f)));
}

TEST_CASE("the outline draws inside the scene's scissor") {
  RecordingCommandList cmd;
  recordOutline(cmd, outlineParams());
  REQUIRE(cmd.scissor_set.x == 10);
  REQUIRE(cmd.scissor_set.width == 300);
}

TEST_CASE("a zero-width outline records nothing") {
  RecordingCommandList cmd;
  MeshOutlineRenderer::DrawParams params = outlineParams();
  params.width = 0.0f;
  recordOutline(cmd, params);
  REQUIRE(cmd.draws.empty());
  REQUIRE(cmd.bound_pipeline == RHI_PIPELINE_INVALID);
}

TEST_CASE("an outline with no depth to read records nothing") {
  RecordingCommandList cmd;
  MeshOutlineRenderer::DrawParams params = outlineParams();
  params.depth = RHI_TEXTURE_INVALID;
  recordOutline(cmd, params);
  REQUIRE(cmd.draws.empty());
}

TEST_CASE("shutting the renderer down gives its pipeline back") {
  FakeDevice device(FakeOutline::PRESENT);
  MeshOutlineRenderer renderer;
  REQUIRE(renderer.init(device));
  renderer.shutdown(device);
  REQUIRE_FALSE(renderer.ready());
  REQUIRE(device.destroyed ==
          std::vector<RhiPipelineHandle>{FAKE_OUTLINE_PIPELINE});
}

TEST_CASE("the crease threshold is a slope change across the sample width") {
  // 0.1 clip units per world unit over an 800 pixel surface is 40 pixels
  // per world unit; depth moves 0.001 per world unit. A crease of the
  // threshold slope, sampled a pixel either side, bends depth by that
  // slope times one pixel's worth of world times depth per world unit.
  const float threshold = MeshOutlineRenderer::creaseThreshold(
      orthoMatrix(0.1f, 0.001f), 800.0f, 1.0f);
  REQUIRE(threshold ==
          Approx(MESH_OUTLINE_CREASE_SLOPE * 0.001f / 40.0f).epsilon(1e-4));
}

TEST_CASE("zooming in lowers the crease threshold with the crease") {
  // Twice the zoom spreads a crease over twice the pixels, halving the
  // bend a fixed sample distance sees; the threshold has to follow or the
  // lines would vanish as the camera closed in.
  const float near = MeshOutlineRenderer::creaseThreshold(
      orthoMatrix(0.2f, 0.001f), SURFACE_W, 1.0f);
  const float far = MeshOutlineRenderer::creaseThreshold(
      orthoMatrix(0.1f, 0.001f), SURFACE_W, 1.0f);
  REQUIRE(near == Approx(far * 0.5f));
}

TEST_CASE("a wider outline samples further and expects a larger bend") {
  const Mat4 m = orthoMatrix(0.1f, 0.001f);
  REQUIRE(
      MeshOutlineRenderer::creaseThreshold(m, SURFACE_W, 2.0f) ==
      Approx(2.0f * MeshOutlineRenderer::creaseThreshold(m, SURFACE_W, 1.0f)));
}

TEST_CASE("an outline narrower than a pixel is sampled a pixel wide") {
  // Depth is read per pixel, so half a pixel either side is the same pixel
  // and would see no bend at all.
  const Mat4 m = orthoMatrix(0.1f, 0.001f);
  REQUIRE(MeshOutlineRenderer::creaseThreshold(m, SURFACE_W, 0.4f) ==
          Approx(MeshOutlineRenderer::creaseThreshold(m, SURFACE_W, 1.0f)));
}

TEST_CASE("a matrix that maps nothing across the screen has no threshold") {
  REQUIRE(MeshOutlineRenderer::creaseThreshold(orthoMatrix(0.0f, 0.001f),
                                               SURFACE_W, 1.0f) == 0.0f);
}
