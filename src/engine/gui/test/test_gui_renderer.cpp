#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <engine/gui/gui-renderer.h>
#include <engine/gui/gui-vertex.h>
#include <engine/render/rhi-buffer-desc.h>
#include <map>
#include <set>
#include <vector>

using namespace eng;

// The GPU reads a frame's GUI geometry while the CPU is already filling the
// next frame's, on every backend. These tests pin down that the renderer
// never writes into a buffer a frame still in flight could be reading.

namespace {

/// Opaque red and blue, packed as the GUI packs colours.
constexpr uint32_t RED = 0xFF0000FFu;
constexpr uint32_t BLUE = 0xFFFF0000u;

/// Quads that overflow the default vertex and index capacities.
constexpr uint32_t OVERFLOW_QUADS = (DEFAULT_VERTEX_CAPACITY / 4) + 1;

/// A device whose buffers are plain memory, so a test can read back what
/// the renderer wrote and see which buffers it gave back.
class FakeBufferDevice final : public RhiDevice {
public:
  RhiBufferHandle createBuffer(const RhiBufferDesc& desc) override {
    const RhiBufferHandle handle = next_handle_++;
    memory[handle].assign(desc.size, 0);
    return handle;
  }
  void destroyBuffer(RhiBufferHandle handle) override {
    memory.erase(handle);
    destroyed.push_back(handle);
  }
  void* mapBuffer(RhiBufferHandle handle) override {
    auto it = memory.find(handle);
    return it == memory.end() ? nullptr : it->second.data();
  }
  void unmapBuffer(RhiBufferHandle /*handle*/) override {}

  /// Bytes behind every live buffer.
  std::map<RhiBufferHandle, std::vector<uint8_t>> memory{};
  /// Every buffer handed back through `destroyBuffer`, in order.
  std::vector<RhiBufferHandle> destroyed{};

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
  void destroyPipeline(RhiPipelineHandle /*handle*/) override {}
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
  /// Next handle `createBuffer` gives out; 0 is invalid.
  RhiBufferHandle next_handle_ = 1;
  /// Capabilities nobody here reads.
  RhiDeviceCapabilities caps_{};
};

/// A command list that remembers which buffers were bound last, and every
/// indexed draw.
class RecordingCommandList final : public RhiCommandList {
public:
  void bindVertexBuffer(RhiBufferHandle buffer, uint64_t /*offset*/) override {
    vertex_buffer = buffer;
  }
  void bindIndexBuffer(RhiBufferHandle buffer, uint64_t /*offset*/,
                       RhiIndexType /*type*/) override {
    index_buffer = buffer;
  }

  void drawIndexed(const RhiDrawIndexedParams& params) override {
    draws.push_back(params);
  }

  /// Last vertex buffer bound.
  RhiBufferHandle vertex_buffer = 0;
  /// Last index buffer bound.
  RhiBufferHandle index_buffer = 0;
  /// Every indexed draw recorded, in order.
  std::vector<RhiDrawIndexedParams> draws{};

  void begin() override {}
  void end() override {}
  void beginRenderPass(const RhiRenderPassBeginInfo& /*info*/) override {}
  void endRenderPass() override {}
  void bindPipeline(RhiPipelineHandle /*pipeline*/) override {}
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

/// One device frame of GUI, as `RenderedGameClient` records it: `quads`
/// quads of `color`, uploaded once and bound.
void drawFrame(GuiRendererContext& gui, RecordingCommandList& cmd,
               uint32_t color, uint32_t quads) {
  gui.beginFrame();
  const Rect rect{0.0f, 0.0f, 10.0f, 10.0f};
  for (uint32_t i = 0; i < quads; ++i) {
    gui.emitQuad({rect, color, 0.0f, 0.0f});
  }
  gui.uploadFrame();
  gui.bindFrame(cmd);
}

/// The vertex buffer each of `frames` consecutive one-quad frames bound.
std::vector<RhiBufferHandle> boundVertexBuffers(GuiRendererContext& gui,
                                                uint32_t frames) {
  RecordingCommandList cmd;
  std::vector<RhiBufferHandle> bound;
  for (uint32_t frame = 0; frame < frames; ++frame) {
    drawFrame(gui, cmd, RED, 1);
    bound.push_back(cmd.vertex_buffer);
  }
  return bound;
}

/// Whether every pair but `slot` still holds the buffers it had.
bool othersUntouched(
    const std::array<GuiFrameBuffers, GUI_FRAME_BUFFER_COUNT>& before,
    const std::array<GuiFrameBuffers, GUI_FRAME_BUFFER_COUNT>& after,
    uint32_t slot) {
  for (uint32_t i = 0; i < GUI_FRAME_BUFFER_COUNT; ++i) {
    if (i != slot && (after[i].vertex_buffer != before[i].vertex_buffer ||
                      after[i].index_buffer != before[i].index_buffer)) {
      return false;
    }
  }
  return true;
}

/// The colour of the first vertex in a vertex buffer.
uint32_t firstVertexColor(const FakeBufferDevice& device,
                          RhiBufferHandle buffer) {
  GuiVertex vertex{};
  std::memcpy(&vertex, device.memory.at(buffer).data(), sizeof(vertex));
  return vertex.color;
}

}  // namespace

TEST_CASE("GuiRendererContext: consecutive frames draw from different buffers",
          "[gui][renderer]") {
  FakeBufferDevice device;
  GuiRendererContext gui;
  REQUIRE(gui.init(&device));
  const auto bound = boundVertexBuffers(gui, GUI_FRAME_BUFFER_COUNT + 1);
  // Every frame in the rotation has its own pair; then it comes round.
  const std::set<RhiBufferHandle> distinct(bound.begin(), bound.end() - 1);
  CHECK(distinct.size() == GUI_FRAME_BUFFER_COUNT);
  CHECK(bound.back() == bound.front());
  std::set<RhiBufferHandle> index_buffers;
  for (const GuiFrameBuffers& pair : gui.frame_buffers) {
    index_buffers.insert(pair.index_buffer);
  }
  CHECK(index_buffers.size() == GUI_FRAME_BUFFER_COUNT);
  gui.shutdown();
}

TEST_CASE("GuiRendererContext: uploading a frame leaves the one before intact",
          "[gui][renderer]") {
  // The race itself: frame A is still being drawn from when frame B is
  // written. With one shared buffer, A's red quad would turn blue.
  FakeBufferDevice device;
  GuiRendererContext gui;
  REQUIRE(gui.init(&device));
  RecordingCommandList cmd;
  drawFrame(gui, cmd, RED, 1);
  const RhiBufferHandle frame_a = cmd.vertex_buffer;
  drawFrame(gui, cmd, BLUE, 1);
  CHECK(cmd.vertex_buffer != frame_a);
  CHECK(firstVertexColor(device, frame_a) == RED);
  CHECK(firstVertexColor(device, cmd.vertex_buffer) == BLUE);
  gui.shutdown();
}

TEST_CASE("GuiRendererContext: growing one frame's buffers leaves the rest",
          "[gui][renderer]") {
  FakeBufferDevice device;
  GuiRendererContext gui;
  REQUIRE(gui.init(&device));
  RecordingCommandList cmd;
  drawFrame(gui, cmd, RED, 1);
  const auto before = gui.frame_buffers;
  drawFrame(gui, cmd, BLUE, OVERFLOW_QUADS);
  const uint32_t grown = gui.frame_slot;
  // Only the pair this frame wrote was replaced, and only its old buffers
  // were given back; the ones earlier frames drew from are untouched.
  CHECK(device.destroyed ==
        std::vector<RhiBufferHandle>{before[grown].vertex_buffer,
                                     before[grown].index_buffer});
  CHECK(gui.frame_buffers[grown].vertex_capacity >= OVERFLOW_QUADS * 4);
  CHECK(othersUntouched(before, gui.frame_buffers, grown));
  gui.shutdown();
}

TEST_CASE("GuiRendererContext: shutdown gives back every frame's buffers",
          "[gui][renderer]") {
  FakeBufferDevice device;
  GuiRendererContext gui;
  REQUIRE(gui.init(&device));
  CHECK(device.memory.size() == 2 * GUI_FRAME_BUFFER_COUNT);
  gui.shutdown();
  CHECK(device.memory.empty());
  CHECK(device.destroyed.size() == 2 * GUI_FRAME_BUFFER_COUNT);
}

TEST_CASE("GuiRendererContext: a later batch draws from its own first index",
          "[gui][renderer]") {
  // Indices name vertices by their place in the whole frame, so a batch is
  // selected by its first index alone. A vertex offset on top counted the
  // batch's start twice: text lost its first glyph on Vulkan and DX12.
  FakeBufferDevice device;
  GuiRendererContext gui;
  REQUIRE(gui.init(&device));
  RecordingCommandList cmd;
  const Rect rect{0.0f, 0.0f, 10.0f, 10.0f};
  gui.beginFrame();
  gui.emitQuad({rect, RED, 0.0f, 0.0f});
  gui.emitTexturedQuad({rect, rect, 7, BLUE});
  gui.uploadFrame();
  gui.bindFrame(cmd);
  gui.submitCommandRange(cmd, 0, gui.commands.size());
  REQUIRE(cmd.draws.size() == 2);
  const RhiDrawIndexedParams& textured = cmd.draws[1];
  CHECK(textured.vertex_offset == 0);
  // The first vertex it draws is the textured quad's own first vertex.
  CHECK(gui.indices.at(textured.first_index) == 4);
  gui.shutdown();
}
