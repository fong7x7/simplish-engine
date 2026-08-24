#include <catch2/catch_test_macros.hpp>

// VulkanDevice tests require the Vulkan SDK.
// Guard with ENGINE_HAS_VULKAN so the file compiles without the SDK.
#ifdef ENGINE_RENDERER_VULKAN

#include "support/render_config_factory.h"

#include <engine/render/backends/vulkan/vulkan-device.h>
#include <engine/render/rhi-types.h>

using namespace eng;
using namespace eng::render;

// Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §1–§7
//   — VulkanDevice: Vulkan 1.3 RhiDevice implementation

// ---------------------------------------------------------------------------
// Factory / Initialization
// ---------------------------------------------------------------------------

TEST_CASE("VulkanDevice: create returns nullopt without valid window",
          "[vulkan][device]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §7
  //   — Swapchain creation fails → create() returns nullopt
  auto config = eng::test::makeTestRenderConfig();
  config.native_window = nullptr;
  auto result = VulkanDevice::create(config);
  // Without a native window, Vulkan surface creation should fail
  REQUIRE_FALSE(result.has_value());
}

TEST_CASE("VulkanDevice: create returns a valid device with valid config",
          "[vulkan][device][integration]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §1.1
  //   — B1: Implement RhiDevice using Vulkan 1.3
  // NOTE: This test requires a running Vulkan-capable system with a window.
  // It will be skipped or fail in headless CI without GPU.
  FAIL("Not implemented — VulkanDevice::create() is a stub");
}

// ---------------------------------------------------------------------------
// Backend identification
// ---------------------------------------------------------------------------

TEST_CASE("VulkanDevice: backend returns VULKAN", "[vulkan][device]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Backend identification
  FAIL("Not implemented — VulkanDevice::backend()");
}

TEST_CASE("VulkanDevice: capabilities populated after create",
          "[vulkan][device]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §3.2
  //   — RhiDeviceCapabilities populated after init
  FAIL("Not implemented — VulkanDevice::capabilities()");
}

// ---------------------------------------------------------------------------
// Buffer lifecycle
// ---------------------------------------------------------------------------

TEST_CASE("VulkanDevice: createBuffer returns valid handle",
          "[vulkan][device]") {
  // Req: docs/engine/rendering.md §2 — Resource creation
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §6.2
  //   — Buffer creation via VMA
  FAIL("Not implemented — VulkanDevice::createBuffer()");
}

TEST_CASE("VulkanDevice: createBuffer host_visible is mappable",
          "[vulkan][device]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  //   — mapBuffer returns non-null for host-visible buffers
  FAIL("Not implemented — VulkanDevice::mapBuffer()");
}

TEST_CASE("VulkanDevice: destroyBuffer with valid handle succeeds",
          "[vulkan][device]") {
  // Req: docs/engine/rendering.md §2 — Resource destruction
  FAIL("Not implemented — VulkanDevice::destroyBuffer()");
}

TEST_CASE("VulkanDevice: destroyBuffer with invalid handle is no-op",
          "[vulkan][device]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §1.2
  //   — E8: destroy with invalid handle is no-op
  FAIL("Not implemented — VulkanDevice::destroyBuffer(invalid)");
}

TEST_CASE("VulkanDevice: mapBuffer on non-host-visible returns nullptr",
          "[vulkan][device]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §6
  //   — Map on non-host-visible buffer returns nullptr
  FAIL("Not implemented — VulkanDevice::mapBuffer(non-host-visible)");
}

// ---------------------------------------------------------------------------
// Texture lifecycle
// ---------------------------------------------------------------------------

TEST_CASE("VulkanDevice: createTexture returns valid handle",
          "[vulkan][device]") {
  // Req: docs/engine/rendering.md §2 — Resource creation
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §6.2
  //   — Texture creation with VkImage + VkImageView
  FAIL("Not implemented — VulkanDevice::createTexture()");
}

TEST_CASE("VulkanDevice: destroyTexture with invalid handle is no-op",
          "[vulkan][device]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §1.2
  //   — E8: destroy with invalid handle is no-op
  FAIL("Not implemented — VulkanDevice::destroyTexture(invalid)");
}

// ---------------------------------------------------------------------------
// Shader lifecycle
// ---------------------------------------------------------------------------

TEST_CASE("VulkanDevice: createShader with valid SPIR-V returns handle",
          "[vulkan][device]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §7
  //   — Shader compilation: SPIR-V for Vulkan
  FAIL("Not implemented — VulkanDevice::createShader()");
}

TEST_CASE("VulkanDevice: createShader with null bytecode returns invalid",
          "[vulkan][device]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §1.2
  //   — E7: corrupt SPIR-V returns RHI_SHADER_INVALID
  FAIL("Not implemented — VulkanDevice::createShader(null)");
}

// ---------------------------------------------------------------------------
// Pipeline lifecycle
// ---------------------------------------------------------------------------

TEST_CASE("VulkanDevice: createGraphicsPipeline returns valid handle",
          "[vulkan][device]") {
  // Req: docs/engine/rendering.md §2 — Pipeline creation
  FAIL("Not implemented — VulkanDevice::createGraphicsPipeline()");
}

TEST_CASE("VulkanDevice: createComputePipeline returns valid handle",
          "[vulkan][device]") {
  // Req: docs/engine/rendering.md §2 — Compute pipeline creation
  FAIL("Not implemented — VulkanDevice::createComputePipeline()");
}

TEST_CASE("VulkanDevice: destroyPipeline with invalid handle is no-op",
          "[vulkan][device]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §1.2
  //   — E8: destroy with invalid handle is no-op
  FAIL("Not implemented — VulkanDevice::destroyPipeline(invalid)");
}

// ---------------------------------------------------------------------------
// Swap chain
// ---------------------------------------------------------------------------

TEST_CASE("VulkanDevice: backbuffer dimensions match config",
          "[vulkan][device]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  //   — backbufferWidth/Height return config values
  FAIL("Not implemented — VulkanDevice::backbufferWidth/Height()");
}

TEST_CASE("VulkanDevice: backbufferTexture returns non-invalid handle",
          "[vulkan][device]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  //   — Current frame's backbuffer handle
  FAIL("Not implemented — VulkanDevice::backbufferTexture()");
}

// ---------------------------------------------------------------------------
// Frame lifecycle
// ---------------------------------------------------------------------------

TEST_CASE("VulkanDevice: beginFrame-endFrame-submit-present cycle",
          "[vulkan][device][integration]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §6.1
  //   — Frame flow: beginFrame → record → submit → endFrame → present
  FAIL("Not implemented — VulkanDevice frame lifecycle");
}

TEST_CASE("VulkanDevice: createCommandList returns non-null",
          "[vulkan][device]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  //   — createCommandList returns a valid command list
  FAIL("Not implemented — VulkanDevice::createCommandList()");
}

// ---------------------------------------------------------------------------
// Capture
// ---------------------------------------------------------------------------

TEST_CASE("VulkanDevice: captureFramebuffer returns result after frame",
          "[vulkan][device][integration]") {
  // Req: docs/engine/rendering.md §2 — Screenshot capture API
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §6.3
  //   — Capture via staging buffer readback
  FAIL("Not implemented — VulkanDevice::captureFramebuffer()");
}

TEST_CASE("VulkanDevice: captureToFile writes file",
          "[vulkan][device][integration]") {
  // Req: docs/engine/rendering.md §2 — Capture to disk
  FAIL("Not implemented — VulkanDevice::captureToFile()");
}

// ---------------------------------------------------------------------------
// Ray tracing extension
// ---------------------------------------------------------------------------

TEST_CASE("VulkanDevice: rayTracing returns nullptr when disabled",
          "[vulkan][device]") {
  // Req: docs/engine/rendering/pipeline.md §3 — Ray tracing extension
  //   — nullptr when unsupported or not requested
  FAIL("Not implemented — VulkanDevice::rayTracing()");
}

// ---------------------------------------------------------------------------
// Synchronization
// ---------------------------------------------------------------------------

TEST_CASE("VulkanDevice: waitIdle drains GPU", "[vulkan][device]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.1
  //   — Drain GPU; call before shutdown or resize
  FAIL("Not implemented — VulkanDevice::waitIdle()");
}

// ---------------------------------------------------------------------------
// Shutdown / cleanup
// ---------------------------------------------------------------------------

TEST_CASE("VulkanDevice: destructor cleans up all resources",
          "[vulkan][device]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §2
  //   — Shutdown flow: waitIdle → destroy resources → destroy device
  FAIL("Not implemented — VulkanDevice destructor");
}

#endif  // ENGINE_RENDERER_VULKAN
