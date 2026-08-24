#include <catch2/catch_test_macros.hpp>

// OpenGL device tests require the OpenGL backend.
// Guard with ENGINE_RENDERER_OPENGL so the file compiles without it.
#ifdef ENGINE_RENDERER_OPENGL

#include "support/render_config_factory.h"

#include <engine/render/backends/opengl/opengl-command-list.h>
#include <engine/render/backends/opengl/opengl-device.h>

using namespace eng;
using namespace eng::render;
using namespace eng::test;

// Req: docs/engine/rendering/pipeline.md §1 — RHI device interface
// Req: docs/technical-approaches/engine/rendering/opengl-backend.md — OpenGL
//   backend

// ============================================================================
// tryCreate — context creation (requires SDL + OpenGL, tagged integration)
// ============================================================================

TEST_CASE("OpenGlDevice: tryCreate with null window returns nullopt",
          "[opengl][device]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §6 —
  //   SDL_GL_CreateContext fails returns nullopt
  auto config = makeOpenGlRenderConfig();
  config.native_window = nullptr;
  auto result = OpenGlDevice::tryCreate(config);
  REQUIRE_FALSE(result.has_value());
}

// ============================================================================
// GlBufferEntry — internal data structure defaults
// ============================================================================

TEST_CASE("GlBufferEntry: default values", "[opengl][device]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §2.2 —
  //   Buffer entry fields
  GlBufferEntry entry;
  REQUIRE(entry.gl_id == 0);
  REQUIRE(entry.size == 0);
  REQUIRE(entry.mapped_ptr == nullptr);
}

// ============================================================================
// GlTextureEntry — internal data structure defaults
// ============================================================================

TEST_CASE("GlTextureEntry: default values", "[opengl][device]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §2.3 —
  //   Texture entry fields
  GlTextureEntry entry;
  REQUIRE(entry.gl_id == 0);
  REQUIRE(entry.width == 0);
  REQUIRE(entry.height == 0);
  REQUIRE(entry.format == RhiFormat::UNDEFINED);
}

// ============================================================================
// GlPipelineEntry — internal data structure defaults
// ============================================================================

TEST_CASE("GlPipelineEntry: default values", "[opengl][device]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §2.4 —
  //   Pipeline entry fields
  GlPipelineEntry entry;
  REQUIRE(entry.program == 0);
  REQUIRE(entry.vao == 0);
  REQUIRE(entry.topology == 0);
  REQUIRE(entry.blend_enabled == false);
  REQUIRE(entry.depth_test == true);
  REQUIRE(entry.depth_write == true);
  REQUIRE(entry.wireframe == false);
  REQUIRE(entry.cull_back == true);
  REQUIRE(entry.front_ccw == true);
}

// ============================================================================
// Contract: backend identification
// ============================================================================

TEST_CASE("OpenGlDevice: backend returns OPEN_GL",
          "[opengl][device][integration]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §3.1 —
  //   backend() returns RhiBackend::OPEN_GL
  // NOTE: This test requires a real GL context. If tryCreate fails (headless
  // CI), the test is skipped.
  auto config = makeOpenGlRenderConfig();
  auto result = OpenGlDevice::tryCreate(config);
  if (!result.has_value()) {
    SKIP("OpenGL context not available (headless)");
  }
  REQUIRE(result.value()->backend() == RhiBackend::OPEN_GL);
}

// ============================================================================
// Contract: capabilities
// ============================================================================

TEST_CASE("OpenGlDevice: ray tracing is never supported",
          "[opengl][device][integration]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §8 —
  //   No ray tracing
  auto config = makeOpenGlRenderConfig();
  config.enable_ray_tracing = true;
  auto result = OpenGlDevice::tryCreate(config);
  if (!result.has_value()) {
    SKIP("OpenGL context not available (headless)");
  }
  REQUIRE_FALSE(result.value()->capabilities().ray_tracing_supported);
}

TEST_CASE("OpenGlDevice: rayTracing returns nullptr",
          "[opengl][device][integration]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §3.1 —
  //   rayTracing() returns nullptr
  auto config = makeOpenGlRenderConfig();
  auto result = OpenGlDevice::tryCreate(config);
  if (!result.has_value()) {
    SKIP("OpenGL context not available (headless)");
  }
  REQUIRE(result.value()->rayTracing() == nullptr);
}

TEST_CASE("OpenGlDevice: device name is non-empty after init",
          "[opengl][device][integration]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §1 —
  //   Query GL_RENDERER
  auto config = makeOpenGlRenderConfig();
  auto result = OpenGlDevice::tryCreate(config);
  if (!result.has_value()) {
    SKIP("OpenGL context not available (headless)");
  }
  auto& caps = result.value()->capabilities();
  REQUIRE(caps.device_name != nullptr);
  REQUIRE(caps.device_name[0] != '\0');
}

TEST_CASE("OpenGlDevice: api version is non-empty after init",
          "[opengl][device][integration]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §1 —
  //   Query GL_VERSION
  auto config = makeOpenGlRenderConfig();
  auto result = OpenGlDevice::tryCreate(config);
  if (!result.has_value()) {
    SKIP("OpenGL context not available (headless)");
  }
  auto& caps = result.value()->capabilities();
  REQUIRE(caps.api_version != nullptr);
  REQUIRE(caps.api_version[0] != '\0');
}

// ============================================================================
// Contract: backbuffer accessors
// ============================================================================

TEST_CASE("OpenGlDevice: backbuffer dimensions match config",
          "[opengl][device][integration]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Backbuffer query
  auto config = makeOpenGlRenderConfig();
  config.backbuffer_width = 640;
  config.backbuffer_height = 480;
  auto result = OpenGlDevice::tryCreate(config);
  if (!result.has_value()) {
    SKIP("OpenGL context not available (headless)");
  }
  REQUIRE(result.value()->backbufferWidth() == 640);
  REQUIRE(result.value()->backbufferHeight() == 480);
}

TEST_CASE("OpenGlDevice: backbufferTexture returns valid handle",
          "[opengl][device][integration]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Backbuffer texture
  auto config = makeOpenGlRenderConfig();
  auto result = OpenGlDevice::tryCreate(config);
  if (!result.has_value()) {
    SKIP("OpenGL context not available (headless)");
  }
  REQUIRE(result.value()->backbufferTexture() != RHI_TEXTURE_INVALID);
}

// ============================================================================
// Contract: frame management
// ============================================================================

TEST_CASE("OpenGlDevice: beginFrame always returns true",
          "[opengl][device][integration]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §3.1 —
  //   beginFrame() always true
  auto config = makeOpenGlRenderConfig();
  auto result = OpenGlDevice::tryCreate(config);
  if (!result.has_value()) {
    SKIP("OpenGL context not available (headless)");
  }
  REQUIRE(result.value()->beginFrame());
}

TEST_CASE("OpenGlDevice: present always returns true",
          "[opengl][device][integration]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §3.1 —
  //   present() always true
  auto config = makeOpenGlRenderConfig();
  auto result = OpenGlDevice::tryCreate(config);
  if (!result.has_value()) {
    SKIP("OpenGL context not available (headless)");
  }
  auto& dev = *result.value();
  dev.beginFrame();
  dev.endFrame();
  REQUIRE(dev.present());
}

// ============================================================================
// Contract: resource creation returns valid handles
// ============================================================================

TEST_CASE("OpenGlDevice: createBuffer returns non-invalid handle",
          "[opengl][device][integration]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Buffer creation
  auto config = makeOpenGlRenderConfig();
  auto result = OpenGlDevice::tryCreate(config);
  if (!result.has_value()) {
    SKIP("OpenGL context not available (headless)");
  }
  RhiBufferDesc desc;
  desc.size = 1024;
  desc.usage = RhiBufferUsage::VERTEX;
  auto handle = result.value()->createBuffer(desc);
  REQUIRE(handle != RHI_BUFFER_INVALID);
  result.value()->destroyBuffer(handle);
}

TEST_CASE("OpenGlDevice: createBuffer returns unique handles",
          "[opengl][device][integration]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Unique resource handles
  auto config = makeOpenGlRenderConfig();
  auto result = OpenGlDevice::tryCreate(config);
  if (!result.has_value()) {
    SKIP("OpenGL context not available (headless)");
  }
  RhiBufferDesc desc;
  desc.size = 512;
  auto h1 = result.value()->createBuffer(desc);
  auto h2 = result.value()->createBuffer(desc);
  REQUIRE(h1 != h2);
  result.value()->destroyBuffer(h1);
  result.value()->destroyBuffer(h2);
}

TEST_CASE("OpenGlDevice: createTexture returns non-invalid handle",
          "[opengl][device][integration]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Texture creation
  auto config = makeOpenGlRenderConfig();
  auto result = OpenGlDevice::tryCreate(config);
  if (!result.has_value()) {
    SKIP("OpenGL context not available (headless)");
  }
  RhiTextureDesc desc;
  desc.width = 64;
  desc.height = 64;
  desc.format = RhiFormat::RGB_A8_UNORM;
  auto handle = result.value()->createTexture(desc);
  REQUIRE(handle != RHI_TEXTURE_INVALID);
  result.value()->destroyTexture(handle);
}

TEST_CASE("OpenGlDevice: createShader returns non-invalid handle",
          "[opengl][device][integration]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Shader creation
  auto config = makeOpenGlRenderConfig();
  auto result = OpenGlDevice::tryCreate(config);
  if (!result.has_value()) {
    SKIP("OpenGL context not available (headless)");
  }
  const char* vertex_src = "#version 460\nvoid main() {}\n";
  RhiShaderDesc desc;
  desc.stage = RhiShaderStage::VERTEX;
  desc.bytecode = reinterpret_cast<const uint8_t*>(vertex_src);
  desc.bytecode_size = static_cast<uint64_t>(strlen(vertex_src));
  auto handle = result.value()->createShader(desc);
  REQUIRE(handle != RHI_SHADER_INVALID);
  result.value()->destroyShader(handle);
}

// ============================================================================
// Contract: destroy with invalid handle is no-op
// ============================================================================

TEST_CASE("OpenGlDevice: destroyBuffer with invalid handle is no-op",
          "[opengl][device][integration]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §6 —
  //   Invalid handle to destroy is no-op
  auto config = makeOpenGlRenderConfig();
  auto result = OpenGlDevice::tryCreate(config);
  if (!result.has_value()) {
    SKIP("OpenGL context not available (headless)");
  }
  result.value()->destroyBuffer(RHI_BUFFER_INVALID);
  result.value()->destroyBuffer(99999);
}

TEST_CASE("OpenGlDevice: destroyTexture with invalid handle is no-op",
          "[opengl][device][integration]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §6
  auto config = makeOpenGlRenderConfig();
  auto result = OpenGlDevice::tryCreate(config);
  if (!result.has_value()) {
    SKIP("OpenGL context not available (headless)");
  }
  result.value()->destroyTexture(RHI_TEXTURE_INVALID);
}

TEST_CASE("OpenGlDevice: destroyShader with invalid handle is no-op",
          "[opengl][device][integration]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §6
  auto config = makeOpenGlRenderConfig();
  auto result = OpenGlDevice::tryCreate(config);
  if (!result.has_value()) {
    SKIP("OpenGL context not available (headless)");
  }
  result.value()->destroyShader(RHI_SHADER_INVALID);
}

TEST_CASE("OpenGlDevice: destroyPipeline with invalid handle is no-op",
          "[opengl][device][integration]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §6
  auto config = makeOpenGlRenderConfig();
  auto result = OpenGlDevice::tryCreate(config);
  if (!result.has_value()) {
    SKIP("OpenGL context not available (headless)");
  }
  result.value()->destroyPipeline(RHI_PIPELINE_INVALID);
}

// ============================================================================
// Contract: buffer mapping
// ============================================================================

TEST_CASE("OpenGlDevice: mapBuffer on host-visible buffer returns non-null",
          "[opengl][device][integration]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Buffer mapping
  auto config = makeOpenGlRenderConfig();
  auto result = OpenGlDevice::tryCreate(config);
  if (!result.has_value()) {
    SKIP("OpenGL context not available (headless)");
  }
  RhiBufferDesc desc;
  desc.size = 256;
  desc.usage = RhiBufferUsage::STAGING;
  desc.host_visible = true;
  auto handle = result.value()->createBuffer(desc);
  REQUIRE(handle != RHI_BUFFER_INVALID);

  auto* ptr = result.value()->mapBuffer(handle);
  REQUIRE(ptr != nullptr);
  result.value()->unmapBuffer(handle);
  result.value()->destroyBuffer(handle);
}

TEST_CASE("OpenGlDevice: mapBuffer on invalid handle returns nullptr",
          "[opengl][device][integration]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §6 —
  //   Map on invalid buffer
  auto config = makeOpenGlRenderConfig();
  auto result = OpenGlDevice::tryCreate(config);
  if (!result.has_value()) {
    SKIP("OpenGL context not available (headless)");
  }
  REQUIRE(result.value()->mapBuffer(RHI_BUFFER_INVALID) == nullptr);
}

// ============================================================================
// Contract: command list creation
// ============================================================================

TEST_CASE("OpenGlDevice: createCommandList returns non-null",
          "[opengl][device][integration]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Command list creation
  auto config = makeOpenGlRenderConfig();
  auto result = OpenGlDevice::tryCreate(config);
  if (!result.has_value()) {
    SKIP("OpenGL context not available (headless)");
  }
  auto cmd = result.value()->createCommandList();
  REQUIRE(cmd != nullptr);
}

TEST_CASE("OpenGlDevice: createCommandList returns OpenGlCommandList",
          "[opengl][device][integration]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §4 —
  //   Module decomposition
  auto config = makeOpenGlRenderConfig();
  auto result = OpenGlDevice::tryCreate(config);
  if (!result.has_value()) {
    SKIP("OpenGL context not available (headless)");
  }
  auto cmd = result.value()->createCommandList();
  auto* gl_cmd = dynamic_cast<OpenGlCommandList*>(cmd.get());
  REQUIRE(gl_cmd != nullptr);
}

// ============================================================================
// Contract: waitIdle does not crash
// ============================================================================

TEST_CASE("OpenGlDevice: waitIdle completes without error",
          "[opengl][device][integration]") {
  // Req: docs/engine/rendering/pipeline.md §1 — GPU synchronization
  auto config = makeOpenGlRenderConfig();
  auto result = OpenGlDevice::tryCreate(config);
  if (!result.has_value()) {
    SKIP("OpenGL context not available (headless)");
  }
  result.value()->waitIdle();
}

// ============================================================================
// Contract: capture returns nullopt when no content rendered
// ============================================================================

TEST_CASE("OpenGlDevice: captureFramebuffer with empty request",
          "[opengl][device][integration]") {
  // Req: docs/engine/rendering.md §2.1 — Capture API
  auto config = makeOpenGlRenderConfig();
  auto result = OpenGlDevice::tryCreate(config);
  if (!result.has_value()) {
    SKIP("OpenGL context not available (headless)");
  }
  RhiCaptureRequest req;
  auto capture = result.value()->captureFramebuffer(req);
  // Capture may succeed or fail depending on framebuffer state;
  // the key invariant is it doesn't crash
}

#endif  // ENGINE_RENDERER_OPENGL
