#ifdef ENGINE_RENDERER_DX12

#include "support/render_config_factory.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <engine/render/backends/dx12/dx12-device.h>
#include <engine/render/rhi-core-types.h>
#include <engine/render/rhi-types.h>

using namespace eng;
using namespace eng::render;

// Req: docs/technical-approaches/engine/rendering/dx12-backend.md §3
//   — Dx12Device: DX12 implementation of the RhiDevice interface

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

TEST_CASE("Dx12Device: create with null window returns nullopt",
          "[dx12][device]") {
  // Req: docs/technical-approaches/engine/rendering/dx12-backend.md §6
  //   — Swapchain creation fails → create() returns nullopt
  RenderConfig config{};
  config.backbuffer_width = 320;
  config.backbuffer_height = 240;
  config.native_window = nullptr;
  auto result = Dx12Device::create(config);
  REQUIRE_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// Backend identification
// ---------------------------------------------------------------------------

TEST_CASE("Dx12Device: backend returns D_X12", "[dx12][device]") {
  // Req: docs/engine/rendering.md §2 — Backend identification
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = Dx12Device::create(config);
  if (!result.has_value()) {
    SKIP("DX12 device creation not available on this system");
  }
  REQUIRE((*result)->backend() == RhiBackend::D_X12);
}

TEST_CASE("Dx12Device: capabilities reports DX12 backend", "[dx12][device]") {
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = Dx12Device::create(config);
  if (!result.has_value()) {
    SKIP("DX12 device creation not available on this system");
  }
  const auto& caps = (*result)->capabilities();
  REQUIRE(caps.backend == RhiBackend::D_X12);
  REQUIRE(caps.compute_supported == true);
  REQUIRE(caps.indirect_draw_supported == true);
  REQUIRE(caps.device_name != nullptr);
  REQUIRE(caps.api_version != nullptr);
}

// ---------------------------------------------------------------------------
// Buffer lifecycle
// ---------------------------------------------------------------------------

TEST_CASE("Dx12Device: createBuffer returns valid handle", "[dx12][device]") {
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = Dx12Device::create(config);
  if (!result.has_value()) {
    SKIP("DX12 device creation not available");
  }
  auto& device = **result;

  RhiBufferDesc desc{};
  desc.size = 1024;
  desc.usage = RhiBufferUsage::VERTEX;
  desc.host_visible = true;

  auto handle = device.createBuffer(desc);
  REQUIRE(handle != RHI_BUFFER_INVALID);
  device.destroyBuffer(handle);
}

TEST_CASE("Dx12Device: destroyBuffer with invalid handle is no-op",
          "[dx12][device]") {
  // Req: docs/engine/rendering.md §2 — destroy*() safe with invalid handles
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = Dx12Device::create(config);
  if (!result.has_value()) {
    SKIP("DX12 device creation not available");
  }
  (*result)->destroyBuffer(RHI_BUFFER_INVALID);
}

TEST_CASE("Dx12Device: mapBuffer returns non-null for host-visible buffer",
          "[dx12][device]") {
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = Dx12Device::create(config);
  if (!result.has_value()) {
    SKIP("DX12 device creation not available");
  }
  auto& device = **result;

  RhiBufferDesc desc{};
  desc.size = 256;
  desc.usage = RhiBufferUsage::VERTEX;
  desc.host_visible = true;

  auto handle = device.createBuffer(desc);
  REQUIRE(handle != RHI_BUFFER_INVALID);

  void* mapped = device.mapBuffer(handle);
  REQUIRE(mapped != nullptr);
  device.unmapBuffer(handle);
  device.destroyBuffer(handle);
}

TEST_CASE("Dx12Device: mapBuffer returns null for non-host-visible buffer",
          "[dx12][device]") {
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = Dx12Device::create(config);
  if (!result.has_value()) {
    SKIP("DX12 device creation not available");
  }
  auto& device = **result;

  RhiBufferDesc desc{};
  desc.size = 256;
  desc.usage = RhiBufferUsage::VERTEX;
  desc.host_visible = false;

  auto handle = device.createBuffer(desc);
  REQUIRE(handle != RHI_BUFFER_INVALID);

  void* mapped = device.mapBuffer(handle);
  REQUIRE(mapped == nullptr);
  device.destroyBuffer(handle);
}

TEST_CASE("Dx12Device: mapBuffer with invalid handle returns null",
          "[dx12][device]") {
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = Dx12Device::create(config);
  if (!result.has_value()) {
    SKIP("DX12 device creation not available");
  }
  REQUIRE((*result)->mapBuffer(RHI_BUFFER_INVALID) == nullptr);
}

// ---------------------------------------------------------------------------
// Texture lifecycle
// ---------------------------------------------------------------------------

TEST_CASE("Dx12Device: createTexture returns valid handle", "[dx12][device]") {
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = Dx12Device::create(config);
  if (!result.has_value()) {
    SKIP("DX12 device creation not available");
  }
  auto& device = **result;

  RhiTextureDesc desc{};
  desc.width = 64;
  desc.height = 64;
  desc.format = RhiFormat::RGB_A8_UNORM;
  desc.usage = RhiTextureUsage::SAMPLED;

  auto handle = device.createTexture(desc);
  REQUIRE(handle != RHI_TEXTURE_INVALID);
  device.destroyTexture(handle);
}

TEST_CASE("Dx12Device: destroyTexture with invalid handle is no-op",
          "[dx12][device]") {
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = Dx12Device::create(config);
  if (!result.has_value()) {
    SKIP("DX12 device creation not available");
  }
  (*result)->destroyTexture(RHI_TEXTURE_INVALID);
}

TEST_CASE("Dx12Device: createTexture render target with RTV",
          "[dx12][device]") {
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = Dx12Device::create(config);
  if (!result.has_value()) {
    SKIP("DX12 device creation not available");
  }
  auto& device = **result;

  RhiTextureDesc desc{};
  desc.width = 64;
  desc.height = 64;
  desc.format = RhiFormat::RGB_A8_UNORM;
  desc.usage = RhiTextureUsage::RENDER_TARGET;

  auto handle = device.createTexture(desc);
  REQUIRE(handle != RHI_TEXTURE_INVALID);
  device.destroyTexture(handle);
}

TEST_CASE("Dx12Device: createTexture depth stencil with DSV",
          "[dx12][device]") {
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = Dx12Device::create(config);
  if (!result.has_value()) {
    SKIP("DX12 device creation not available");
  }
  auto& device = **result;

  RhiTextureDesc desc{};
  desc.width = 64;
  desc.height = 64;
  desc.format = RhiFormat::D32_FLOAT;
  desc.usage = RhiTextureUsage::DEPTH_STENCIL;

  auto handle = device.createTexture(desc);
  REQUIRE(handle != RHI_TEXTURE_INVALID);
  device.destroyTexture(handle);
}

// ---------------------------------------------------------------------------
// Shader lifecycle
// ---------------------------------------------------------------------------

TEST_CASE("Dx12Device: createShader with null bytecode returns invalid",
          "[dx12][device]") {
  // Req: docs/technical-approaches/engine/rendering/dx12-backend.md §6
  //   — Shader bytecode invalid → RHI_SHADER_INVALID
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = Dx12Device::create(config);
  if (!result.has_value()) {
    SKIP("DX12 device creation not available");
  }
  RhiShaderDesc desc{};
  desc.bytecode = nullptr;
  desc.bytecode_size = 0;
  REQUIRE((*result)->createShader(desc) == RHI_SHADER_INVALID);
}

TEST_CASE("Dx12Device: createShader with valid bytecode returns handle",
          "[dx12][device]") {
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = Dx12Device::create(config);
  if (!result.has_value()) {
    SKIP("DX12 device creation not available");
  }
  uint8_t dummy_bytecode[] = {0x44, 0x58, 0x42, 0x43};
  RhiShaderDesc desc{};
  desc.bytecode = dummy_bytecode;
  desc.bytecode_size = sizeof(dummy_bytecode);
  desc.stage = RhiShaderStage::VERTEX;

  auto handle = (*result)->createShader(desc);
  REQUIRE(handle != RHI_SHADER_INVALID);
  (*result)->destroyShader(handle);
}

TEST_CASE("Dx12Device: destroyShader with invalid handle is no-op",
          "[dx12][device]") {
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = Dx12Device::create(config);
  if (!result.has_value()) {
    SKIP("DX12 device creation not available");
  }
  (*result)->destroyShader(RHI_SHADER_INVALID);
}

// ---------------------------------------------------------------------------
// Pipeline lifecycle
// ---------------------------------------------------------------------------

TEST_CASE("Dx12Device: createGraphicsPipeline with invalid shaders returns "
          "invalid",
          "[dx12][device]") {
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = Dx12Device::create(config);
  if (!result.has_value()) {
    SKIP("DX12 device creation not available");
  }
  RhiGraphicsPipelineDesc desc{};
  desc.vertex_shader = RHI_SHADER_INVALID;
  desc.fragment_shader = RHI_SHADER_INVALID;
  REQUIRE((*result)->createGraphicsPipeline(desc) == RHI_PIPELINE_INVALID);
}

TEST_CASE("Dx12Device: createComputePipeline with invalid shader returns "
          "invalid",
          "[dx12][device]") {
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = Dx12Device::create(config);
  if (!result.has_value()) {
    SKIP("DX12 device creation not available");
  }
  RhiComputePipelineDesc desc{};
  desc.compute_shader = RHI_SHADER_INVALID;
  REQUIRE((*result)->createComputePipeline(desc) == RHI_PIPELINE_INVALID);
}

TEST_CASE("Dx12Device: destroyPipeline with invalid handle is no-op",
          "[dx12][device]") {
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = Dx12Device::create(config);
  if (!result.has_value()) {
    SKIP("DX12 device creation not available");
  }
  (*result)->destroyPipeline(RHI_PIPELINE_INVALID);
}

// ---------------------------------------------------------------------------
// Swap chain
// ---------------------------------------------------------------------------

TEST_CASE("Dx12Device: backbuffer dimensions match config", "[dx12][device]") {
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = Dx12Device::create(config);
  if (!result.has_value()) {
    SKIP("DX12 device creation not available");
  }
  REQUIRE((*result)->backbufferWidth() == config.backbuffer_width);
  REQUIRE((*result)->backbufferHeight() == config.backbuffer_height);
}

TEST_CASE("Dx12Device: resizeSwapchain with zero dimensions is no-op",
          "[dx12][device]") {
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = Dx12Device::create(config);
  if (!result.has_value()) {
    SKIP("DX12 device creation not available");
  }
  auto w = (*result)->backbufferWidth();
  auto h = (*result)->backbufferHeight();
  (*result)->resizeSwapchain(0, 0);
  REQUIRE((*result)->backbufferWidth() == w);
  REQUIRE((*result)->backbufferHeight() == h);
}

// ---------------------------------------------------------------------------
// Frame management
// ---------------------------------------------------------------------------

TEST_CASE("Dx12Device: beginFrame succeeds", "[dx12][device]") {
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = Dx12Device::create(config);
  if (!result.has_value()) {
    SKIP("DX12 device creation not available");
  }
  REQUIRE((*result)->beginFrame() == true);
  (*result)->waitIdle();
}

TEST_CASE("Dx12Device: createCommandList returns non-null", "[dx12][device]") {
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = Dx12Device::create(config);
  if (!result.has_value()) {
    SKIP("DX12 device creation not available");
  }
  (*result)->beginFrame();
  auto cmd = (*result)->createCommandList();
  REQUIRE(cmd != nullptr);
  (*result)->waitIdle();
}

// ---------------------------------------------------------------------------
// Extensions
// ---------------------------------------------------------------------------

TEST_CASE("Dx12Device: rayTracing returns nullptr", "[dx12][device]") {
  // Req: docs/technical-approaches/engine/rendering/dx12-backend.md §1
  //   — DXR 1.1 is optional; returns nullptr when unavailable
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = Dx12Device::create(config);
  if (!result.has_value()) {
    SKIP("DX12 device creation not available");
  }
  REQUIRE((*result)->rayTracing() == nullptr);
}

// ---------------------------------------------------------------------------
// Synchronization
// ---------------------------------------------------------------------------

TEST_CASE("Dx12Device: waitIdle does not crash", "[dx12][device]") {
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = Dx12Device::create(config);
  if (!result.has_value()) {
    SKIP("DX12 device creation not available");
  }
  (*result)->waitIdle();
}

#endif  // ENGINE_RENDERER_DX12
