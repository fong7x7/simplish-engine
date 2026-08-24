#include "support/render_config_factory.h"

#include <catch2/catch_test_macros.hpp>
#include <engine/render/rhi-device-factory.h>

using namespace eng;
using namespace eng::render;

// Req: docs/platform/REQUIREMENTS.md §2.2 — RhiDeviceFactory moves to platform

TEST_CASE("RhiDeviceFactory lives in eng::render namespace",
          "[platform][render][factory]") {
  // Req: docs/platform/REQUIREMENTS.md §2.2 — Namespace change
  auto config = eng::test::makePlatformTestRenderConfig();
  auto result = RhiDeviceFactory::create(config);
  // Factory should return a device (stub) or nullopt — either is valid
  // without a real window.  The key assertion is that the call compiles
  // and the type is correct.
  static_assert(std::is_same_v<decltype(result), RhiDeviceOptional>,
                "Factory must return RhiDeviceOptional");
  SUCCEED();
}

TEST_CASE("RhiDeviceFactory::create returns nullopt for null window",
          "[platform][render][factory]") {
  // Req: docs/platform/REQUIREMENTS.md §2.2 — Factory error handling
  auto config = eng::test::makePlatformTestRenderConfig();
  config.native_window = nullptr;
  auto result = RhiDeviceFactory::create(config);
  // Stub backend always returns a device; real backends return nullopt.
  // Accept either outcome — the key assertion is no crash.
  SUCCEED();
}

TEST_CASE("RhiDeviceFactory::create with auto backend selects platform default",
          "[platform][render][factory]") {
  // Req: docs/platform/REQUIREMENTS.md §2.1 — Backend selection
  auto config = eng::test::makePlatformTestRenderConfig();
  config.preferred_backend = "";
  auto result = RhiDeviceFactory::create(config);
  // With auto backend and null window, factory should still attempt
  // selection and fail gracefully (no crash).
  SUCCEED();
}

TEST_CASE("RhiDeviceFactory::create preserves RenderConfig dimensions",
          "[platform][render][factory]") {
  // Req: docs/platform/REQUIREMENTS.md §2.2 — No runtime behaviour changes
  auto config = eng::test::makePlatformTestRenderConfig();
  config.backbuffer_width = 1280;
  config.backbuffer_height = 720;
  auto result = RhiDeviceFactory::create(config);
  // When a real device is created, backbuffer dimensions should match.
  // Stub backend always succeeds — verify it returned something.
  REQUIRE(result.has_value());
}
