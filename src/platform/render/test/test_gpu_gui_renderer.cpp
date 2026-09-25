#include <catch2/catch_test_macros.hpp>

// The GUI renderer on whichever GPU backend the build selected, drawing into
// an offscreen target that is read back. Skips where the machine has no
// device for it, or the backend has no built-in GUI pipeline.

#include "support/gpu_test_context.h"

#include <array>
#include <cstdint>
#include <engine/gui/gui-renderer.h>
#include <engine/gui/gui-vertex.h>
#include <engine/render/rhi-render-pass-begin-info.h>
#include <engine/render/rhi-texture-desc.h>
#include <vector>

using namespace eng;
using eng::test::GPU_TEST_SIZE;
using eng::test::GpuTestContext;

// Req: docs/platform/REQUIREMENTS.md PLT-RHI-1 — one RHI draw means the same
// thing on every backend. The GUI's batches pass a first index into an index
// buffer of frame-wide vertex numbers; Metal used to drop the first index and
// Vulkan and DX12 were handed a vertex offset on top, so each backend drew a
// different subset of the frame's quads.

namespace {

/// Opaque green and white, packed as the GUI packs colours.
constexpr uint32_t GREEN = 0xFF00FF00u;
constexpr uint32_t WHITE = 0xFFFFFFFFu;

/// Half the target, in layout pixels.
constexpr float HALF = GPU_TEST_SIZE / 2.0f;

/// A row through the middle of the target.
constexpr uint32_t MID_ROW = GPU_TEST_SIZE / 2;

/// The BGRA texel at (x, y), row 0 at the top.
std::array<int, 4> texelAt(const std::vector<uint8_t>& texels, uint32_t x,
                           uint32_t y) {
  const size_t i = (static_cast<size_t>(y) * GPU_TEST_SIZE + x) * 4;
  return {texels[i], texels[i + 1], texels[i + 2], texels[i + 3]};
}

/// Whether `device` is real and ships the GUI pipeline — Metal's stub
/// fallback, used when no GPU answers, does not.
bool hasGuiPipeline(RhiDevice* device) {
  RhiPipelineHandle probe = RHI_PIPELINE_INVALID;
  if (device == nullptr || !device->tryCreateGuiPipeline(probe)) {
    return false;
  }
  device->destroyPipeline(probe);
  return true;
}

/// A 1x1 texture of opaque blue.
RhiTextureHandle createBlueTexture(RhiDevice& device) {
  static constexpr std::array<uint8_t, 4> BLUE_RGBA{0, 0, 255, 255};
  RhiTextureDesc desc{};
  desc.format = RhiFormat::RGB_A8_UNORM;
  desc.usage = RhiTextureUsage::SAMPLED;
  desc.initial_pixels = BLUE_RGBA.data();
  return device.createTexture(desc);
}

/// Two batches: a solid green quad on the left, then — so its batch starts
/// part-way into the frame's buffers — a blue-textured quad on the right.
void emitTwoBatches(GuiRendererContext& gui, RhiTextureHandle blue) {
  const Rect left{0.0f, 0.0f, HALF, GPU_TEST_SIZE};
  const Rect right{HALF, 0.0f, HALF, GPU_TEST_SIZE};
  const Rect whole_uv{0.0f, 0.0f, 1.0f, 1.0f};
  gui.viewport_width = GPU_TEST_SIZE;
  gui.viewport_height = GPU_TEST_SIZE;
  gui.beginFrame();
  gui.emitQuad({left, GREEN, 0.0f, 0.0f});
  gui.emitTexturedQuad({right, whole_uv, blue, WHITE});
}

/// What `RenderedGameClient` records for one GUI pass.
std::vector<uint8_t> renderGui(const GpuTestContext& ctx,
                               GuiRendererContext& gui) {
  const RhiTextureHandle target = ctx.createColorTarget();
  return ctx.renderAndRead(target, [&](RhiCommandList& cmd) {
    RhiRenderPassBeginInfo rp{};
    rp.color_targets = &target;
    rp.color_target_count = 1;
    cmd.beginRenderPass(rp);
    gui.uploadFrame();
    gui.bindFrame(cmd);
    gui.submitCommandRange(cmd, 0, gui.commands.size());
    cmd.endRenderPass();
  });
}

}  // namespace

TEST_CASE("GuiRendererContext on the GPU: every batch draws its own quads",
          "[gpu][gui]") {
  GpuTestContext ctx;
  if (!hasGuiPipeline(ctx.device())) {
    SKIP("no GPU device with a built-in GUI pipeline");
  }
  GuiRendererContext gui;
  REQUIRE(gui.init(ctx.device()));
  emitTwoBatches(gui, createBlueTexture(*ctx.device()));
  const auto texels = renderGui(ctx, gui);
  REQUIRE_FALSE(texels.empty());
  CHECK(texelAt(texels, GPU_TEST_SIZE / 4, 8) ==
        std::array<int, 4>{0, 255, 0, 255});
  CHECK(texelAt(texels, 3 * GPU_TEST_SIZE / 4, 8) ==
        std::array<int, 4>{255, 0, 0, 255});
  gui.shutdown();
}

namespace {

/// Opaque red, blue and white, packed as the GUI packs colours.
constexpr uint32_t RED = 0xFF0000FFu;
constexpr uint32_t BLUE = 0xFFFF0000u;

/// The whole target, in layout pixels.
constexpr Rect WHOLE{0.0f, 0.0f, GPU_TEST_SIZE, GPU_TEST_SIZE};

/// Start a frame the size of the target.
void beginTargetFrame(GuiRendererContext& gui) {
  gui.viewport_width = GPU_TEST_SIZE;
  gui.viewport_height = GPU_TEST_SIZE;
  gui.beginFrame();
}

/// The GPU's GUI context, or a skip when there is none.
struct GuiOnGpu {
  GpuTestContext ctx;
  GuiRendererContext gui;
  bool ready = false;

  GuiOnGpu() {
    ready = hasGuiPipeline(ctx.device()) && gui.init(ctx.device());
    if (ready) {
      beginTargetFrame(gui);
    }
  }
  GuiOnGpu(const GuiOnGpu&) = delete;
  GuiOnGpu& operator=(const GuiOnGpu&) = delete;
  GuiOnGpu(GuiOnGpu&&) = delete;
  GuiOnGpu& operator=(GuiOnGpu&&) = delete;
  ~GuiOnGpu() {
    if (ready) {
      gui.shutdown();
    }
  }
};

/// White across the target, and on it the black shadow of a 20-pixel box
/// at the centre, blurred 8: its quad is the box grown by the blur.
void emitShadowOnWhite(GuiRendererContext& gui) {
  gui.emitQuad({WHOLE, WHITE, 0.0f, 0.0f});
  gui.emitShape({14.0f, 14.0f, 36.0f, 36.0f},
                {.color = 0xFF000000u,
                 .color2 = 0xFF000000u,
                 .flags = GUI_VERTEX_SHAPE | GUI_VERTEX_SHADOW,
                 .param = 8.0f});
}

}  // namespace

TEST_CASE("GuiRendererContext on the GPU: a linear gradient runs its angle",
          "[gpu][gui]") {
  GuiOnGpu gpu;
  if (!gpu.ready) {
    SKIP("no GPU device with a built-in GUI pipeline");
  }
  // Angle 0: left to right, red to blue.
  gpu.gui.emitShape(WHOLE, {.color = RED,
                            .color2 = BLUE,
                            .flags = GUI_VERTEX_LINEAR_GRADIENT,
                            .param = 0.0f});
  const auto texels = renderGui(gpu.ctx, gpu.gui);
  const auto left = texelAt(texels, 1, MID_ROW);
  const auto right = texelAt(texels, GPU_TEST_SIZE - 2, MID_ROW);
  // BGRA: red is index 2, blue index 0. A pixel in, the gradient is a
  // couple of percent along, which sRGB encoding lifts to about 42.
  CHECK(left[2] > 230);
  CHECK(left[0] < 60);
  CHECK(right[0] > 230);
  CHECK(right[2] < 60);
}

TEST_CASE("GuiRendererContext on the GPU: each corner takes its own radius",
          "[gpu][gui]") {
  GuiOnGpu gpu;
  if (!gpu.ready) {
    SKIP("no GPU device with a built-in GUI pipeline");
  }
  gpu.gui.emitShape(WHOLE, {.color = WHITE,
                            .color2 = WHITE,
                            .radii = {24.0f, 0.0f, 0.0f, 0.0f},
                            .flags = GUI_VERTEX_SHAPE});
  const auto texels = renderGui(gpu.ctx, gpu.gui);
  // The rounded top-left corner shows the black clear; the square
  // top-right is painted.
  CHECK(texelAt(texels, 1, 1)[0] < 20);
  CHECK(texelAt(texels, GPU_TEST_SIZE - 2, 1)[0] > 240);
}

TEST_CASE("GuiRendererContext on the GPU: a border is only as wide as its "
          "side",
          "[gpu][gui]") {
  GuiOnGpu gpu;
  if (!gpu.ready) {
    SKIP("no GPU device with a built-in GUI pipeline");
  }
  // Only a left border, 10 pixels.
  gpu.gui.emitShape(
      WHOLE,
      {.color = WHITE, .color2 = WHITE, .border = {0.0f, 0.0f, 0.0f, 10.0f}});
  const auto texels = renderGui(gpu.ctx, gpu.gui);
  CHECK(texelAt(texels, 4, MID_ROW)[0] > 240);
  CHECK(texelAt(texels, 20, MID_ROW)[0] < 20);
  CHECK(texelAt(texels, GPU_TEST_SIZE - 2, MID_ROW)[0] < 20);
}

TEST_CASE("GuiRendererContext on the GPU: a shadow fades out from its shape",
          "[gpu][gui]") {
  GuiOnGpu gpu;
  if (!gpu.ready) {
    SKIP("no GPU device with a built-in GUI pipeline");
  }
  emitShadowOnWhite(gpu.gui);
  const auto texels = renderGui(gpu.ctx, gpu.gui);
  const std::array<int, 3> blue{texelAt(texels, 32, 32)[0],
                                texelAt(texels, 44, 32)[0],
                                texelAt(texels, 60, 32)[0]};
  // Inside, at the edge, and well beyond it.
  CHECK(blue[0] < 20);
  CHECK((blue[1] > blue[0] && blue[1] < blue[2]));
  CHECK(blue[2] > 240);
}
