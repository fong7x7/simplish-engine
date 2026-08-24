#include "support/metal_config_factory.h"

#include <catch2/catch_test_macros.hpp>
#include <engine/render/backends/metal/metal-rhi-config.h>

using namespace eng;
using namespace eng::test;

// Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §2
// Req: docs/engine/rendering.md §2 — RHI concepts

TEST_CASE("MetalRhiConfig: default values are sensible", "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §2.1
  MetalRhiConfig cfg;
  REQUIRE(cfg.max_frames_in_flight == 2);
  REQUIRE(cfg.native_window == nullptr);
  REQUIRE(cfg.backbuffer_width == 1920);
  REQUIRE(cfg.backbuffer_height == 1080);
  REQUIRE(cfg.vsync == true);
  REQUIRE(cfg.enable_validation == false);
  REQUIRE(cfg.enable_ray_tracing == false);
}

TEST_CASE("MetalRhiConfig: test factory produces small config",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §2.1
  auto cfg = makeTestMetalRhiConfig();
  REQUIRE(cfg.backbuffer_width == 320);
  REQUIRE(cfg.backbuffer_height == 240);
  REQUIRE(cfg.vsync == false);
  REQUIRE(cfg.enable_validation == false);
  REQUIRE(cfg.enable_ray_tracing == false);
  REQUIRE(cfg.native_window == nullptr);
}

TEST_CASE("MetalRhiConfig: ray tracing factory enables ray tracing",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §1 —
  // MR9
  auto cfg = makeTestMetalRhiConfigWithRayTracing();
  REQUIRE(cfg.enable_ray_tracing == true);
}

TEST_CASE("MetalRhiConfig: validation factory enables validation",
          "[platform-macos]") {
  // Req: docs/technical-approaches/engine/rendering/metal-rhi-backend.md §6 —
  // Validation
  auto cfg = makeTestMetalRhiConfigWithValidation();
  REQUIRE(cfg.enable_validation == true);
}
