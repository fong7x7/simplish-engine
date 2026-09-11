#include <catch2/catch_test_macros.hpp>

// VulkanDevice tests need a Vulkan driver (MoltenVK on macOS). Each one that
// needs a device skips when none can be made, so the suite still runs where
// there is no GPU (PLT-RHI-5).
#ifdef ENGINE_RENDERER_VULKAN

#include "support/gpu_test_context.h"
#include "support/render_config_factory.h"

#include <array>
#include <cstdint>
#include <engine/render/backends/vulkan/vulkan-device.h>
#include <engine/render/rhi-buffer-desc.h>
#include <engine/render/rhi-capture-request.h>
#include <engine/render/rhi-render-pass-begin-info.h>
#include <engine/render/rhi-shader-desc.h>
#include <engine/render/rhi-texture-desc.h>
#include <engine/render/rhi-texture-update-2d.h>
#include <engine/render/rhi-types.h>
#include <stb_image.h>

using namespace eng;
using namespace eng::render;
using eng::test::GpuTestContext;

// Req: docs/platform/REQUIREMENTS.md PLT-RHI-1 — every backend implements
// the whole RhiDevice interface with the Metal backend's observable
// semantics.

namespace {

/// Whether a test buffer is one the CPU can map.
enum class Mapping { HOST_VISIBLE, DEVICE_LOCAL };

RhiBufferHandle makeBuffer(RhiDevice& device, uint64_t size, Mapping mapping) {
  RhiBufferDesc desc{};
  desc.size = size;
  desc.usage = RhiBufferUsage::VERTEX;
  desc.host_visible = mapping == Mapping::HOST_VISIBLE;
  return device.createBuffer(desc);
}

/// A whole-texture update of a 2x2 RGBA8 texture.
RhiTextureUpdate2D fullUpdate(const void* pixels) {
  RhiTextureUpdate2D update{};
  update.pixels = pixels;
  update.width = 2;
  update.height = 2;
  update.format = RhiFormat::RGB_A8_UNORM;
  return update;
}

/// The first pixel of a PNG capture, RGBA, or all -1 if it will not
/// decode.
std::array<int, 4> firstCapturedPixel(const RhiCaptureResult& capture) {
  int w = 0;
  int h = 0;
  int channels = 0;
  stbi_uc* rgba = stbi_load_from_memory(capture.data.data(),
                                        static_cast<int>(capture.data.size()),
                                        &w, &h, &channels, 4);
  if (rgba == nullptr) {
    return {-1, -1, -1, -1};
  }
  const std::array<int, 4> pixel{rgba[0], rgba[1], rgba[2], rgba[3]};
  stbi_image_free(rgba);
  return pixel;
}

RhiTextureHandle makeRgbaTexture(RhiDevice& device, const void* pixels) {
  RhiTextureDesc desc{};
  desc.width = 2;
  desc.height = 2;
  desc.format = RhiFormat::RGB_A8_UNORM;
  desc.usage = RhiTextureUsage::SAMPLED;
  desc.initial_pixels = pixels;
  return device.createTexture(desc);
}

/// Begin a frame, clear the back buffer to red, and submit it, leaving it
/// to the caller to present.
bool submitRedFrame(RhiDevice& device) {
  if (!device.beginFrame()) {
    return false;
  }
  auto cmd = device.createCommandList();
  cmd->begin();
  const RhiTextureHandle backbuffer = device.backbufferTexture();
  RhiRenderPassBeginInfo rp{};
  rp.color_targets = &backbuffer;
  rp.color_target_count = 1;
  rp.clear_color[0] = 1.0f;
  cmd->beginRenderPass(rp);
  cmd->endRenderPass();
  cmd->end();
  device.submit(*cmd);
  device.endFrame();
  return true;
}

/// One whole red frame, presented.
bool presentRedFrame(RhiDevice& device) {
  return submitRedFrame(device) && device.present();
}

}  // namespace

TEST_CASE("VulkanDevice: create returns nullopt without a window",
          "[vulkan][device]") {
  auto config = eng::test::makeTestRenderConfig();
  config.native_window = nullptr;
  REQUIRE_FALSE(VulkanDevice::create(config).has_value());
}

TEST_CASE("VulkanDevice: reports the Vulkan backend and its limits",
          "[vulkan][device][gpu]") {
  GpuTestContext ctx;
  if (ctx.device() == nullptr) {
    SKIP("no Vulkan device on this machine");
  }
  const RhiDeviceCapabilities& caps = ctx.device()->capabilities();
  CHECK(ctx.device()->backend() == RhiBackend::VULKAN);
  CHECK(caps.backend == RhiBackend::VULKAN);
  CHECK(caps.max_texture_dimension_2d > 0);
  CHECK(caps.device_name != nullptr);
  CHECK(ctx.device()->rayTracing() == nullptr);
}

TEST_CASE("VulkanDevice: host-visible buffers map, device-local ones do not",
          "[vulkan][device][gpu]") {
  GpuTestContext ctx;
  if (ctx.device() == nullptr) {
    SKIP("no Vulkan device on this machine");
  }
  RhiDevice& device = *ctx.device();
  const RhiBufferHandle shared = makeBuffer(device, 256, Mapping::HOST_VISIBLE);
  const RhiBufferHandle local = makeBuffer(device, 256, Mapping::DEVICE_LOCAL);
  REQUIRE(shared != RHI_BUFFER_INVALID);
  REQUIRE(local != RHI_BUFFER_INVALID);
  CHECK(device.mapBuffer(shared) != nullptr);
  device.unmapBuffer(shared);
  CHECK(device.mapBuffer(local) == nullptr);
  device.destroyBuffer(shared);
  device.destroyBuffer(local);
}

TEST_CASE("VulkanDevice: destroying an unknown handle does nothing",
          "[vulkan][device][gpu]") {
  GpuTestContext ctx;
  if (ctx.device() == nullptr) {
    SKIP("no Vulkan device on this machine");
  }
  RhiDevice& device = *ctx.device();
  device.destroyBuffer(RHI_BUFFER_INVALID);
  device.destroyTexture(0xDEADBEEFULL);
  device.destroyShader(RHI_SHADER_INVALID);
  device.destroyPipeline(0xDEADBEEFULL);
  const RhiBufferHandle buffer = makeBuffer(device, 64, Mapping::HOST_VISIBLE);
  device.destroyBuffer(buffer);
  device.destroyBuffer(buffer);
  SUCCEED("no crash, and validation (when enabled) reports nothing");
}

TEST_CASE("VulkanDevice: updateTexture2D checks format and bounds",
          "[vulkan][device][gpu]") {
  GpuTestContext ctx;
  if (ctx.device() == nullptr) {
    SKIP("no Vulkan device on this machine");
  }
  const std::array<uint8_t, 16> pixels{};
  const RhiTextureHandle tex = makeRgbaTexture(*ctx.device(), pixels.data());
  REQUIRE(tex != RHI_TEXTURE_INVALID);
  RhiTextureUpdate2D update = fullUpdate(pixels.data());
  CHECK(ctx.device()->updateTexture2D(tex, update));
  update.offset_x = 1;
  CHECK_FALSE(ctx.device()->updateTexture2D(tex, update));
  update.offset_x = 0;
  update.format = RhiFormat::R8_UNORM;
  CHECK_FALSE(ctx.device()->updateTexture2D(tex, update));
}

TEST_CASE("VulkanDevice: createShader rejects missing or unaligned SPIR-V",
          "[vulkan][device][gpu]") {
  GpuTestContext ctx;
  if (ctx.device() == nullptr) {
    SKIP("no Vulkan device on this machine");
  }
  RhiShaderDesc desc{};
  CHECK(ctx.device()->createShader(desc) == RHI_SHADER_INVALID);
  alignas(4) const std::array<uint8_t, 9> bytes{};
  desc.bytecode = bytes.data() + 1;
  desc.bytecode_size = 8;
  CHECK(ctx.device()->createShader(desc) == RHI_SHADER_INVALID);
}

TEST_CASE("VulkanDevice: builds all four built-in pipelines",
          "[vulkan][device][gpu]") {
  // Req: the built-in pipelines Metal compiles from MSL and DX12 from HLSL
  // exist here too, compiled from GLSL (vulkan-builtin-pipelines.h).
  GpuTestContext ctx;
  if (ctx.device() == nullptr) {
    SKIP("no Vulkan device on this machine");
  }
  RhiDevice& device = *ctx.device();
  std::array<RhiPipelineHandle, 4> pipelines{};
  CHECK(device.tryCreateGuiPipeline(pipelines[0]));
  CHECK(device.tryCreateMeshPipeline(pipelines[1]));
  CHECK(device.tryCreateSkinnedMeshPipeline(pipelines[2]));
  CHECK(device.tryCreateMeshOutlinePipeline(pipelines[3]));
  for (const RhiPipelineHandle pipeline : pipelines) {
    CHECK(pipeline != RHI_PIPELINE_INVALID);
    device.destroyPipeline(pipeline);
  }
}

TEST_CASE("VulkanDevice: a frame begins, submits and presents",
          "[vulkan][device][gpu]") {
  GpuTestContext ctx;
  if (ctx.device() == nullptr) {
    SKIP("no Vulkan device on this machine");
  }
  RhiDevice& device = *ctx.device();
  CHECK(device.backbufferWidth() > 0);
  CHECK(device.backbufferHeight() > 0);
  // More frames than are in flight, so each slot's fence is waited on.
  for (int frame = 0; frame < 4; ++frame) {
    REQUIRE(presentRedFrame(device));
    CHECK(device.backbufferTexture() != RHI_TEXTURE_INVALID);
  }
}

TEST_CASE("VulkanDevice: captureFramebuffer returns the frame as RGBA PNG",
          "[vulkan][device][gpu]") {
  GpuTestContext ctx;
  if (ctx.device() == nullptr) {
    SKIP("no Vulkan device on this machine");
  }
  REQUIRE(submitRedFrame(*ctx.device()));
  auto capture = ctx.device()->captureFramebuffer(RhiCaptureRequest{});
  REQUIRE(ctx.device()->present());
  REQUIRE(capture.has_value());
  CHECK(capture->width == ctx.device()->backbufferWidth());
  // The swapchain is BGRA; a capture that forgot to swizzle comes back blue.
  const std::array<int, 4> pixel = firstCapturedPixel(*capture);
  CHECK(pixel == std::array<int, 4>{255, 0, 0, 255});
}

TEST_CASE("VulkanDevice: the back buffer cannot be captured once presented",
          "[vulkan][device][gpu]") {
  // Presented, the image is the presentation engine's until acquired again.
  GpuTestContext ctx;
  if (ctx.device() == nullptr) {
    SKIP("no Vulkan device on this machine");
  }
  REQUIRE(presentRedFrame(*ctx.device()));
  CHECK_FALSE(
      ctx.device()->captureFramebuffer(RhiCaptureRequest{}).has_value());
}

TEST_CASE("VulkanDevice: a resource destroyed mid-frame outlives the frame",
          "[vulkan][device][gpu]") {
  // A Metal command buffer retains what it encodes; the GUI regrows its
  // vertex buffer mid-stream relying on that. Vulkan has to hold the buffer
  // until the frame that bound it is done.
  GpuTestContext ctx;
  if (ctx.device() == nullptr) {
    SKIP("no Vulkan device on this machine");
  }
  RhiDevice& device = *ctx.device();
  const RhiTextureHandle target = ctx.createColorTarget();
  const RhiBufferHandle buffer =
      makeBuffer(device, 1024, Mapping::HOST_VISIBLE);
  ctx.renderAndRead(target, [&](RhiCommandList& cmd) {
    cmd.bindVertexBuffer(buffer);
    device.destroyBuffer(buffer);
  });
  for (int frame = 0; frame < 3; ++frame) {
    REQUIRE(presentRedFrame(device));
  }
}

TEST_CASE("VulkanDevice: resizeSwapchain leaves a swapchain to draw to",
          "[vulkan][device][gpu]") {
  GpuTestContext ctx;
  if (ctx.device() == nullptr) {
    SKIP("no Vulkan device on this machine");
  }
  ctx.device()->resizeSwapchain(eng::test::GPU_TEST_SIZE,
                                eng::test::GPU_TEST_SIZE);
  CHECK(presentRedFrame(*ctx.device()));
  ctx.device()->resizeSwapchain(0, 0);
  CHECK(presentRedFrame(*ctx.device()));
}

#endif  // ENGINE_RENDERER_VULKAN
