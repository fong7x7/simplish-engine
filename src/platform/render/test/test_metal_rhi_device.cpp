#include "support/metal_config_factory.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <engine/render/backends/metal/metal-rhi-device.h>
#include <engine/render/rhi-capture-request.h>
#include <engine/render/rhi-command-list.h>
#include <engine/render/rhi-device.h>
#include <engine/render/rhi-texture-update-2d.h>
#include <engine/render/rhi-types.h>

using namespace eng;
using namespace eng::test;

namespace {

/// Create a Metal RHI device for tests; asserts creation succeeds.
std::unique_ptr<RhiDevice> makeTestDevice() {
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  return std::move(*result);
}

/// Create a RGBA8 SAMPLED texture of the given size on the device.
RhiTextureHandle makeRgba8Texture(RhiDevice& device, uint32_t w, uint32_t h) {
  RhiTextureDesc desc;
  desc.width = w;
  desc.height = h;
  desc.format = RhiFormat::RGB_A8_UNORM;
  desc.usage = RhiTextureUsage::SAMPLED;
  return device.createTexture(desc);
}

/// Create a 2x2 SAMPLED texture of the given format on the device.
RhiTextureHandle makeSampledTexture(RhiDevice& device, RhiFormat fmt) {
  RhiTextureDesc desc;
  desc.width = 2;
  desc.height = 2;
  desc.format = fmt;
  desc.usage = RhiTextureUsage::SAMPLED;
  return device.createTexture(desc);
}

/// Build a RhiTextureUpdate2D with the given pixel buffer and dimensions.
RhiTextureUpdate2D makeUpdate2D(const uint8_t* pixels, uint32_t w, uint32_t h,
                                RhiFormat fmt = RhiFormat::RGB_A8_UNORM) {
  RhiTextureUpdate2D update;
  update.pixels = pixels;
  update.width = w;
  update.height = h;
  update.format = fmt;
  return update;
}

}  // namespace

// ==========================================================================
// Factory / Initialization
// ==========================================================================

TEST_CASE("MetalRhiDevice: create returns a valid device", "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §3.1
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  REQUIRE(*result != nullptr);
}

// ==========================================================================
// Backend Identification
// ==========================================================================

TEST_CASE("MetalRhiDevice: backend returns METAL", "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §1 —
  // MR1
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  REQUIRE((*result)->backend() == RhiBackend::METAL);
}

TEST_CASE("MetalRhiDevice: capabilities reports METAL backend",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §1.5
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  const auto& caps = (*result)->capabilities();
  REQUIRE(caps.backend == RhiBackend::METAL);
}

TEST_CASE("MetalRhiDevice: compute is always supported", "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §1 —
  // MR8
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  REQUIRE((*result)->capabilities().compute_supported == true);
}

TEST_CASE("MetalRhiDevice: device name is non-empty", "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §3.2
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  const auto* name = (*result)->capabilities().device_name;
  REQUIRE(name != nullptr);
  REQUIRE(name[0] != '\0');
}

// ==========================================================================
// Buffer Lifecycle
// ==========================================================================

TEST_CASE("MetalRhiDevice: createBuffer returns valid handle",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  auto& device = **result;

  RhiBufferDesc desc;
  desc.size = 1024;
  desc.usage = RhiBufferUsage::VERTEX;
  desc.host_visible = true;

  auto handle = device.createBuffer(desc);
  REQUIRE(handle != RHI_BUFFER_INVALID);

  device.destroyBuffer(handle);
}

TEST_CASE("MetalRhiDevice: createBuffer returns unique handles",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  auto& device = **result;

  RhiBufferDesc desc;
  desc.size = 256;
  desc.usage = RhiBufferUsage::UNIFORM;
  desc.host_visible = true;

  auto h1 = device.createBuffer(desc);
  auto h2 = device.createBuffer(desc);
  REQUIRE(h1 != RHI_BUFFER_INVALID);
  REQUIRE(h2 != RHI_BUFFER_INVALID);
  REQUIRE(h1 != h2);

  device.destroyBuffer(h1);
  device.destroyBuffer(h2);
}

TEST_CASE("MetalRhiDevice: destroyBuffer with invalid handle is no-op",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  (*result)->destroyBuffer(RHI_BUFFER_INVALID);
}

TEST_CASE("MetalRhiDevice: mapBuffer returns non-null for host-visible",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §5.1
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  auto& device = **result;

  RhiBufferDesc desc;
  desc.size = 512;
  desc.usage = RhiBufferUsage::VERTEX;
  desc.host_visible = true;

  auto handle = device.createBuffer(desc);
  REQUIRE(handle != RHI_BUFFER_INVALID);

  void* ptr = device.mapBuffer(handle);
  REQUIRE(ptr != nullptr);
  device.unmapBuffer(handle);
  device.destroyBuffer(handle);
}

// ==========================================================================
// Texture Lifecycle
// ==========================================================================

TEST_CASE("MetalRhiDevice: createTexture returns valid handle",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  auto& device = **result;

  RhiTextureDesc desc;
  desc.width = 64;
  desc.height = 64;
  desc.format = RhiFormat::RGB_A8_SRGB;
  desc.usage = RhiTextureUsage::SAMPLED;

  auto handle = device.createTexture(desc);
  REQUIRE(handle != RHI_TEXTURE_INVALID);

  device.destroyTexture(handle);
}

TEST_CASE("MetalRhiDevice: destroyTexture with invalid handle is no-op",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  (*result)->destroyTexture(RHI_TEXTURE_INVALID);
}

// ==========================================================================
// Shader Lifecycle
// ==========================================================================

TEST_CASE("MetalRhiDevice: createShader returns valid handle",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §1 —
  // MR6
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  auto& device = **result;

  // Minimal dummy bytecode for stub — real bytecode would be metallib.
  const uint8_t dummy_bytecode[] = {0xDE, 0xAD};
  RhiShaderDesc desc;
  desc.stage = RhiShaderStage::VERTEX;
  desc.bytecode = dummy_bytecode;
  desc.bytecode_size = sizeof(dummy_bytecode);
  desc.entry_point = "main";

  auto handle = device.createShader(desc);
  REQUIRE(handle != RHI_SHADER_INVALID);

  device.destroyShader(handle);
}

// ==========================================================================
// Pipeline Lifecycle
// ==========================================================================

TEST_CASE(
    "MetalRhiDevice: createGraphicsPipeline returns valid handle",
    // Algorithm: MetalRhiDevice: createGraphicsPipeline returns valid handle
    "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §5.1
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  auto& device = **result;

  // Create shaders first.
  const uint8_t dummy[] = {0xDE, 0xAD};
  RhiShaderDesc vs_desc;
  vs_desc.stage = RhiShaderStage::VERTEX;
  vs_desc.bytecode = dummy;
  vs_desc.bytecode_size = sizeof(dummy);
  auto vs = device.createShader(vs_desc);

  RhiShaderDesc fs_desc;
  fs_desc.stage = RhiShaderStage::FRAGMENT;
  fs_desc.bytecode = dummy;
  fs_desc.bytecode_size = sizeof(dummy);
  auto fs = device.createShader(fs_desc);

  RhiGraphicsPipelineDesc desc;
  desc.vertex_shader = vs;
  desc.fragment_shader = fs;

  auto handle = device.createGraphicsPipeline(desc);
  REQUIRE(handle != RHI_PIPELINE_INVALID);

  device.destroyPipeline(handle);
  device.destroyShader(vs);
  device.destroyShader(fs);
}

TEST_CASE("MetalRhiDevice: createComputePipeline returns valid handle",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §1 —
  // MR8
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  auto& device = **result;

  const uint8_t dummy[] = {0xDE, 0xAD};
  RhiShaderDesc cs_desc;
  cs_desc.stage = RhiShaderStage::COMPUTE;
  cs_desc.bytecode = dummy;
  cs_desc.bytecode_size = sizeof(dummy);
  auto cs = device.createShader(cs_desc);

  RhiComputePipelineDesc desc;
  desc.compute_shader = cs;

  auto handle = device.createComputePipeline(desc);
  REQUIRE(handle != RHI_PIPELINE_INVALID);

  device.destroyPipeline(handle);
  device.destroyShader(cs);
}

TEST_CASE("MetalRhiDevice: destroyPipeline with invalid handle is no-op",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  (*result)->destroyPipeline(RHI_PIPELINE_INVALID);
}

// ==========================================================================
// Swap Chain / Backbuffer
// ==========================================================================

TEST_CASE("MetalRhiDevice: backbufferWidth matches config",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  REQUIRE((*result)->backbufferWidth() == config.backbuffer_width);
}

TEST_CASE("MetalRhiDevice: backbufferHeight matches config",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  REQUIRE((*result)->backbufferHeight() == config.backbuffer_height);
}

TEST_CASE("MetalRhiDevice: backbufferTexture returns valid handle",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  REQUIRE((*result)->backbufferTexture() != RHI_TEXTURE_INVALID);
}

// ==========================================================================
// Frame Management
// ==========================================================================

TEST_CASE("MetalRhiDevice: beginFrame returns true", "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §1.5
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  REQUIRE((*result)->beginFrame() == true);
  (*result)->endFrame();
}

TEST_CASE("MetalRhiDevice: present returns true", "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §1.5
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  auto& device = **result;
  REQUIRE(device.beginFrame() == true);
  device.endFrame();
  REQUIRE(device.present() == true);
}

TEST_CASE("MetalRhiDevice: full frame cycle with command list",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §1.5
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  auto& device = **result;

  REQUIRE(device.beginFrame() == true);
  auto cmd = device.createCommandList();
  REQUIRE(cmd != nullptr);
  cmd->begin();
  cmd->end();
  device.submit(*cmd);
  device.endFrame();
  REQUIRE(device.present() == true);
}

// ==========================================================================
// Command List
// ==========================================================================

TEST_CASE("MetalRhiDevice: createCommandList returns non-null",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  auto cmd = (*result)->createCommandList();
  REQUIRE(cmd != nullptr);
}

TEST_CASE("MetalRhiDevice: command list render pass lifecycle",
          // Algorithm: MetalRhiDevice: command list render pass lifecycle
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §5.2
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  auto& device = **result;

  device.beginFrame();
  auto cmd = device.createCommandList();
  cmd->begin();

  RhiTextureHandle backbuffer = device.backbufferTexture();
  RhiRenderPassBeginInfo rp_info;
  rp_info.color_targets = &backbuffer;
  rp_info.color_target_count = 1;
  rp_info.color_load_op = RhiLoadOp::CLEAR;

  cmd->beginRenderPass(rp_info);
  cmd->endRenderPass();
  cmd->end();
  device.submit(*cmd);
  device.endFrame();
  device.present();
}

TEST_CASE("MetalRhiDevice: command list draw inside render pass",
          // Algorithm: MetalRhiDevice: command list draw inside render pass
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §5.2
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  auto& device = **result;

  // Create pipeline resources.
  const uint8_t dummy[] = {0xDE, 0xAD};
  RhiShaderDesc vs_desc;
  vs_desc.stage = RhiShaderStage::VERTEX;
  vs_desc.bytecode = dummy;
  vs_desc.bytecode_size = sizeof(dummy);
  auto vs = device.createShader(vs_desc);

  RhiShaderDesc fs_desc;
  fs_desc.stage = RhiShaderStage::FRAGMENT;
  fs_desc.bytecode = dummy;
  fs_desc.bytecode_size = sizeof(dummy);
  auto fs = device.createShader(fs_desc);

  RhiGraphicsPipelineDesc pipe_desc;
  pipe_desc.vertex_shader = vs;
  pipe_desc.fragment_shader = fs;
  auto pipeline = device.createGraphicsPipeline(pipe_desc);

  RhiBufferDesc buf_desc;
  buf_desc.size = 256;
  buf_desc.usage = RhiBufferUsage::VERTEX;
  auto vb = device.createBuffer(buf_desc);

  device.beginFrame();
  auto cmd = device.createCommandList();
  cmd->begin();

  RhiTextureHandle backbuffer = device.backbufferTexture();
  RhiRenderPassBeginInfo rp_info;
  rp_info.color_targets = &backbuffer;
  rp_info.color_target_count = 1;

  cmd->beginRenderPass(rp_info);
  cmd->bindPipeline(pipeline);
  cmd->bindVertexBuffer(vb);
  cmd->setViewport({0.0f, 0.0f, 320.0f, 240.0f, 0.0f, 1.0f});
  cmd->setScissor({0, 0, 320, 240});
  cmd->draw({3, 1, 0, 0});
  cmd->endRenderPass();
  cmd->end();

  device.submit(*cmd);
  device.endFrame();
  device.present();

  device.destroyBuffer(vb);
  device.destroyPipeline(pipeline);
  device.destroyShader(vs);
  device.destroyShader(fs);
}

// Algorithm: MetalRhiDevice: command list compute dispatch
TEST_CASE("MetalRhiDevice: command list compute dispatch", "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §1 —
  // MR8
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  auto& device = **result;

  const uint8_t dummy[] = {0xDE, 0xAD};
  RhiShaderDesc cs_desc;
  cs_desc.stage = RhiShaderStage::COMPUTE;
  cs_desc.bytecode = dummy;
  cs_desc.bytecode_size = sizeof(dummy);
  auto cs = device.createShader(cs_desc);

  RhiComputePipelineDesc pipe_desc;
  pipe_desc.compute_shader = cs;
  auto pipeline = device.createComputePipeline(pipe_desc);

  device.beginFrame();
  auto cmd = device.createCommandList();
  cmd->begin();
  cmd->bindPipeline(pipeline);
  cmd->dispatch(1, 1, 1);
  cmd->end();
  device.submit(*cmd);
  device.endFrame();
  device.present();

  device.destroyPipeline(pipeline);
  device.destroyShader(cs);
}

// Algorithm: MetalRhiDevice: command list buffer copy
TEST_CASE("MetalRhiDevice: command list buffer copy", "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.2
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  auto& device = **result;

  RhiBufferDesc desc;
  desc.size = 256;
  desc.usage = RhiBufferUsage::STAGING;
  desc.host_visible = true;
  auto src = device.createBuffer(desc);
  auto dst = device.createBuffer(desc);

  device.beginFrame();
  auto cmd = device.createCommandList();
  cmd->begin();
  cmd->copyBuffer({src, dst, 256, 0, 0});
  cmd->end();
  device.submit(*cmd);
  device.endFrame();
  device.present();

  device.destroyBuffer(src);
  device.destroyBuffer(dst);
}

// Algorithm: MetalRhiDevice: command list texture barrier
TEST_CASE("MetalRhiDevice: command list texture barrier", "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §7.2
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  auto& device = **result;

  RhiTextureDesc tex_desc;
  tex_desc.width = 64;
  tex_desc.height = 64;
  tex_desc.format = RhiFormat::RGB_A8_SRGB;
  tex_desc.usage = RhiTextureUsage::SAMPLED | RhiTextureUsage::RENDER_TARGET;
  auto tex = device.createTexture(tex_desc);

  device.beginFrame();
  auto cmd = device.createCommandList();
  cmd->begin();
  cmd->textureBarrier(tex, RhiTextureLayout::UNDEFINED,
                      RhiTextureLayout::SHADER_READ_ONLY);
  cmd->end();
  device.submit(*cmd);
  device.endFrame();
  device.present();

  device.destroyTexture(tex);
}

// ==========================================================================
// Capture API
// ==========================================================================

TEST_CASE("MetalRhiDevice: captureFramebuffer returns result",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §5.3
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  auto& device = **result;

  device.beginFrame();
  device.endFrame();
  device.present();

  RhiCaptureRequest request;
  request.format = RhiCaptureFormat::PNG;
  auto capture = device.captureFramebuffer(request);
  // Stub may return nullopt; real backend returns data.
  // The important thing is it doesn't crash.
  if (capture.has_value()) {
    REQUIRE(capture->width > 0);
    REQUIRE(capture->height > 0);
  }
}

// ==========================================================================
// Optional Extensions
// ==========================================================================

TEST_CASE("MetalRhiDevice: rayTracing returns nullptr when not requested",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §1 —
  // MR9
  auto config = makeTestMetalRhiConfig();
  config.enable_ray_tracing = false;
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  REQUIRE((*result)->rayTracing() == nullptr);
}

// ==========================================================================
// Synchronization
// ==========================================================================

TEST_CASE("MetalRhiDevice: waitIdle does not crash", "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  (*result)->waitIdle();
}

// ==========================================================================
// Unified Memory Query
// ==========================================================================

// Note: MetalRhiDevice::hasUnifiedMemory() is on the wrapper class whose
// construction is internal (private ctor). It is tested as part of the real
// Metal .mm backend integration, not through the stub path.
// See: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §7.1

// ==========================================================================
// Resize Swapchain
// ==========================================================================

TEST_CASE("MetalRhiDevice: resizeSwapchain updates backbuffer dimensions",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  auto& device = **result;

  device.resizeSwapchain(640, 480);
  REQUIRE(device.backbufferWidth() == 640);
  REQUIRE(device.backbufferHeight() == 480);
}

TEST_CASE("MetalRhiDevice: resizeSwapchain with zero width is no-op",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  auto& device = **result;

  auto orig_w = device.backbufferWidth();
  auto orig_h = device.backbufferHeight();
  device.resizeSwapchain(0, 480);
  REQUIRE(device.backbufferWidth() == orig_w);
  REQUIRE(device.backbufferHeight() == orig_h);
}

TEST_CASE("MetalRhiDevice: resizeSwapchain with zero height is no-op",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  auto& device = **result;

  auto orig_w = device.backbufferWidth();
  auto orig_h = device.backbufferHeight();
  device.resizeSwapchain(640, 0);
  REQUIRE(device.backbufferWidth() == orig_w);
  REQUIRE(device.backbufferHeight() == orig_h);
}

// ==========================================================================
// Texture Update 2D
// ==========================================================================

TEST_CASE("MetalRhiDevice: updateTexture2D succeeds with valid params",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  auto device = makeTestDevice();
  auto tex = makeRgba8Texture(*device, 4, 4);
  REQUIRE(tex != RHI_TEXTURE_INVALID);

  std::array<uint8_t, 64> pixels{};
  auto update = makeUpdate2D(pixels.data(), 4, 4);

  REQUIRE(device->updateTexture2D(tex, update));
  device->destroyTexture(tex);
}

TEST_CASE("MetalRhiDevice: updateTexture2D with subregion offset",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  auto device = makeTestDevice();
  auto tex = makeRgba8Texture(*device, 8, 8);

  std::array<uint8_t, 16> pixels{};
  auto update = makeUpdate2D(pixels.data(), 2, 2);
  update.offset_x = 2;
  update.offset_y = 2;

  REQUIRE(device->updateTexture2D(tex, update));
  device->destroyTexture(tex);
}

TEST_CASE("MetalRhiDevice: updateTexture2D fails with invalid handle",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  auto device = makeTestDevice();

  std::array<uint8_t, 16> pixels{};
  auto update = makeUpdate2D(pixels.data(), 2, 2);

  REQUIRE_FALSE(device->updateTexture2D(RHI_TEXTURE_INVALID, update));
}

TEST_CASE("MetalRhiDevice: updateTexture2D fails with format mismatch",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  auto device = makeTestDevice();
  auto tex = makeRgba8Texture(*device, 4, 4);

  std::array<uint8_t, 8> pixels{};
  auto update = makeUpdate2D(pixels.data(), 4, 4, RhiFormat::R8_UNORM);

  REQUIRE_FALSE(device->updateTexture2D(tex, update));
  device->destroyTexture(tex);
}

TEST_CASE("MetalRhiDevice: updateTexture2D fails with out-of-bounds region",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  auto device = makeTestDevice();
  auto tex = makeRgba8Texture(*device, 4, 4);

  std::array<uint8_t, 64> pixels{};
  auto update = makeUpdate2D(pixels.data(), 4, 1);
  update.offset_x = 3;

  REQUIRE_FALSE(device->updateTexture2D(tex, update));
  device->destroyTexture(tex);
}

TEST_CASE("MetalRhiDevice: updateTexture2D fails with null pixels",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  auto device = makeTestDevice();
  auto tex = makeRgba8Texture(*device, 4, 4);

  auto update = makeUpdate2D(nullptr, 4, 4);

  REQUIRE_FALSE(device->updateTexture2D(tex, update));
  device->destroyTexture(tex);
}

// ==========================================================================
// Texture Creation Edge Cases
// ==========================================================================

TEST_CASE("MetalRhiDevice: createTexture with initial pixels copies data",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  auto& device = **result;

  std::array<uint8_t, 16> pixels{};
  pixels.fill(0xAB);

  RhiTextureDesc desc;
  desc.width = 2;
  desc.height = 2;
  desc.format = RhiFormat::RGB_A8_UNORM;
  desc.usage = RhiTextureUsage::SAMPLED;
  desc.initial_pixels = pixels.data();

  auto handle = device.createTexture(desc);
  REQUIRE(handle != RHI_TEXTURE_INVALID);
  device.destroyTexture(handle);
}

TEST_CASE(
    "MetalRhiDevice: createTexture with compressed format returns invalid",
    "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  auto& device = **result;

  RhiTextureDesc desc;
  desc.width = 4;
  desc.height = 4;
  desc.format = RhiFormat::B_C7_UNORM;
  desc.usage = RhiTextureUsage::SAMPLED;

  auto handle = device.createTexture(desc);
  REQUIRE(handle == RHI_TEXTURE_INVALID);
}

TEST_CASE("MetalRhiDevice: createTexture with various formats succeeds",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  auto device = makeTestDevice();
  const RhiFormat formats[] = {
      RhiFormat::R8_UNORM,          RhiFormat::R_G8_UNORM,
      RhiFormat::RGB_A8_SRGB,       RhiFormat::BGR_A8_SRGB,
      RhiFormat::R16_FLOAT,         RhiFormat::R_G16_FLOAT,
      RhiFormat::R32_FLOAT,         RhiFormat::D32_FLOAT,
      RhiFormat::RGB_A16_FLOAT,     RhiFormat::R_G32_FLOAT,
      RhiFormat::D32_FLOAT_S8_UINT, RhiFormat::RGB_A32_FLOAT,
      RhiFormat::D16_UNORM,         RhiFormat::D24_UNORM_S8_UINT};

  for (auto fmt : formats) {
    auto handle = makeSampledTexture(*device, fmt);
    REQUIRE(handle != RHI_TEXTURE_INVALID);
    device->destroyTexture(handle);
  }
}

// ==========================================================================
// GUI Pipeline Stub
// ==========================================================================

TEST_CASE("MetalRhiDevice: tryCreateGuiPipeline returns false",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §5.1
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  auto& device = **result;

  RhiPipelineHandle pipeline = RHI_PIPELINE_INVALID;
  REQUIRE_FALSE(device.tryCreateGuiPipeline(pipeline));
}

// ==========================================================================
// Capture To File
// ==========================================================================

TEST_CASE("MetalRhiDevice: captureToFile returns false in stub",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §5.3
  auto config = makeTestMetalRhiConfig();
  auto result = MetalRhiDevice::create(config);
  REQUIRE(result.has_value());
  auto& device = **result;

  RhiCaptureRequest request;
  request.format = RhiCaptureFormat::PNG;
  REQUIRE_FALSE(device.captureToFile(request, "/tmp/test.png"));
}
