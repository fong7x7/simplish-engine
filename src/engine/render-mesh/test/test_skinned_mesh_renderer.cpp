#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <engine/render-mesh/mesh-fragment-lights.h>
#include <engine/render-mesh/skin-palette.h>
#include <engine/render-mesh/skinned-mesh-renderer.h>
#include <map>
#include <vector>

using namespace eng;

namespace {

/// The handle the fake backend gives its skinned pipeline.
constexpr RhiPipelineHandle FAKE_SKINNED_PIPELINE = 77;
/// The handle the fake backend gives the one texture it is asked for.
constexpr RhiTextureHandle FAKE_STAND_IN = 5;
/// A texture a test instance names as its own.
constexpr RhiTextureHandle OWN_TEXTURE = 9;

/// Which backend the fake stands in for: one with a skinned pipeline, or one
/// without, which is Vulkan and the stub today.
enum class FakeSkinning : uint8_t { PRESENT, ABSENT };

/// Just enough of a device to hand out a pipeline, a texture, and buffers
/// that can be mapped and written.
class FakeDevice final : public RhiDevice {
public:
  explicit FakeDevice(FakeSkinning skinning) : skinning_(skinning) {}

  bool tryCreateSkinnedMeshPipeline(RhiPipelineHandle& out) override {
    if (skinning_ == FakeSkinning::ABSENT) {
      return false;
    }
    out = FAKE_SKINNED_PIPELINE;
    return true;
  }
  RhiBufferHandle createBuffer(const RhiBufferDesc& desc) override {
    buffers_.emplace_back(desc.size);
    return buffers_.size();
  }
  void* mapBuffer(RhiBufferHandle handle) override {
    return buffers_[handle - 1].data();
  }
  RhiTextureHandle createTexture(const RhiTextureDesc& /*desc*/) override {
    return FAKE_STAND_IN;
  }
  void destroyBuffer(RhiBufferHandle handle) override {
    destroyed_buffers += handle != 0 ? 1 : 0;
  }
  void destroyPipeline(RhiPipelineHandle /*handle*/) override {}

  /// How many real buffers were handed back.
  int destroyed_buffers = 0;

  RhiBackend backend() const override { return RhiBackend::STUB; }
  const RhiDeviceCapabilities& capabilities() const override { return caps_; }
  void unmapBuffer(RhiBufferHandle /*handle*/) override {}
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
  /// Whether this backend has a skinned pipeline to give.
  FakeSkinning skinning_;
  /// Every buffer's bytes, handle `n` at index `n - 1`.
  std::vector<std::vector<uint8_t>> buffers_{};
  /// Capabilities nobody here reads.
  RhiDeviceCapabilities caps_{};
};

/// A command list that remembers what the skinned pass asked of it.
class RecordingCommandList final : public RhiCommandList {
public:
  void bindPipeline(RhiPipelineHandle pipeline) override {
    bound_pipeline = pipeline;
  }
  void setVertexStageBytes(const void* data, size_t size,
                           uint32_t slot) override {
    auto& bytes = vertex_bytes[slot];
    bytes.resize(size);
    std::memcpy(bytes.data(), data, size);
  }
  void setFragmentStageBytes(const void* data, size_t size,
                             uint32_t slot) override {
    fragment_slot = slot;
    fragment_bytes.resize(size);
    std::memcpy(fragment_bytes.data(), data, size);
  }
  void bindFragmentTexture(RhiTextureHandle texture,
                           uint32_t /*slot*/) override {
    texture_bound = texture;
  }
  void drawIndexed(const RhiDrawIndexedParams& params) override {
    index_counts.push_back(params.index_count);
  }

  /// Last pipeline bound.
  RhiPipelineHandle bound_pipeline = RHI_PIPELINE_INVALID;
  /// Last vertex stage payload, per slot.
  std::map<uint32_t, std::vector<uint8_t>> vertex_bytes{};
  /// Last fragment stage payload.
  std::vector<uint8_t> fragment_bytes{};
  /// Slot that payload went to.
  uint32_t fragment_slot = 99;
  /// Last texture bound to the fragment stage.
  RhiTextureHandle texture_bound = RHI_TEXTURE_INVALID;
  /// Index count of every indexed draw recorded.
  std::vector<uint32_t> index_counts{};

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
  void setScissor(const RhiScissor& /*scissor*/) override {}
  void draw(const RhiDrawParams& /*params*/) override {}
  void dispatch(uint32_t /*x*/, uint32_t /*y*/, uint32_t /*z*/) override {}
  void copyBuffer(const RhiCopyBufferParams& /*params*/) override {}
  void copyTextureToBuffer(RhiTextureHandle /*src*/,
                           RhiBufferHandle /*dst*/) override {}
  void textureBarrier(RhiTextureHandle /*texture*/, RhiTextureLayout /*from*/,
                      RhiTextureLayout /*to*/) override {}
};

/// One skinned triangle.
SkinnedMeshData triangle() {
  SkinnedMeshData mesh;
  mesh.vertices.resize(3);
  mesh.indices = {0, 1, 2};
  return mesh;
}

/// A renderer that has uploaded one triangle, and the id it got.
struct Uploaded {
  /// The device the renderer runs on.
  FakeDevice device{FakeSkinning::PRESENT};
  /// The renderer.
  SkinnedMeshRenderer renderer;
  /// The triangle's id.
  MeshGpuId mesh = MESH_GPU_INVALID;

  Uploaded() {
    REQUIRE(renderer.init(device));
    mesh = renderer.upload(device, triangle()).value_or(MESH_GPU_INVALID);
  }
};

/// Draw @p instance alone with the given renderer.
void drawOne(const Uploaded& up, const SkinnedMeshInstance& instance,
             RecordingCommandList& cmd) {
  SkinnedMeshRenderer::DrawParams params{};
  params.instances = std::span(&instance, 1);
  params.shade_bands = 3;
  up.renderer.draw(cmd, params);
}

}  // namespace

TEST_CASE("a backend without a skinned pipeline leaves the renderer inert") {
  FakeDevice device(FakeSkinning::ABSENT);
  SkinnedMeshRenderer renderer;
  REQUIRE_FALSE(renderer.init(device));
  REQUIRE_FALSE(renderer.ready());
  RecordingCommandList cmd;
  const SkinnedMeshInstance instance{};
  renderer.draw(cmd, {.instances = std::span(&instance, 1)});
  REQUIRE(cmd.bound_pipeline == RHI_PIPELINE_INVALID);
}

TEST_CASE("an empty skinned mesh is not uploaded") {
  Uploaded up;
  REQUIRE_FALSE(up.renderer.upload(up.device, SkinnedMeshData{}).has_value());
  REQUIRE(up.renderer.meshCount() == 1);
}

TEST_CASE("a skinned draw sends its matrices, palette, and lights") {
  Uploaded up;
  std::vector<Mat4> skin{Mat4::identity()};
  skin[0](0, 3) = 6.0f;
  RecordingCommandList cmd;
  drawOne(up, {up.mesh, Mat4::identity(), RHI_TEXTURE_INVALID, skin}, cmd);
  REQUIRE(cmd.bound_pipeline == FAKE_SKINNED_PIPELINE);
  REQUIRE(cmd.vertex_bytes[1].size() == 2 * sizeof(Mat4));
  REQUIRE(cmd.vertex_bytes[2].size() == sizeof(SkinPalette));
  SkinPalette sent{};
  std::memcpy(&sent, cmd.vertex_bytes[2].data(), sizeof(sent));
  REQUIRE(sent.rows[0][3] == 6.0f);
  REQUIRE(cmd.fragment_slot == 0);
  REQUIRE(cmd.fragment_bytes.size() == sizeof(MeshFragmentLights));
  REQUIRE(cmd.index_counts == std::vector<uint32_t>{3});
}

TEST_CASE("a skinned instance with no texture takes the stand-in") {
  Uploaded up;
  RecordingCommandList cmd;
  drawOne(up, {up.mesh, Mat4::identity(), RHI_TEXTURE_INVALID, {}}, cmd);
  REQUIRE(cmd.texture_bound == FAKE_STAND_IN);
  drawOne(up, {up.mesh, Mat4::identity(), OWN_TEXTURE, {}}, cmd);
  REQUIRE(cmd.texture_bound == OWN_TEXTURE);
}

TEST_CASE("a skinned instance naming an unknown mesh is skipped") {
  Uploaded up;
  RecordingCommandList cmd;
  drawOne(up, {up.mesh + 10, Mat4::identity(), RHI_TEXTURE_INVALID, {}}, cmd);
  REQUIRE(cmd.index_counts.empty());
}

TEST_CASE("shutting down releases every uploaded skinned mesh") {
  Uploaded up;
  up.renderer.shutdown(up.device);
  REQUIRE(up.device.destroyed_buffers == 2);
  REQUIRE(up.renderer.meshCount() == 0);
  REQUIRE_FALSE(up.renderer.ready());
}
