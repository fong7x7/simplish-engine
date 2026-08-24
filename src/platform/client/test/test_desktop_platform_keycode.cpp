#include <catch2/catch_test_macros.hpp>
#include <engine/client/desktop-platform-keycode.h>

using namespace eng::client;

// Req: docs/platform/REQUIREMENTS.md §2.3 — DesktopPlatformKeycode moves with
//      DesktopGameClient

TEST_CASE("DesktopPlatformKeycode::ESCAPE matches SDL3 SDLK_ESCAPE",
          "[platform][client][keycode]") {
  // Req: docs/platform/REQUIREMENTS.md §2.3 — Keycode constants
  REQUIRE(DesktopPlatformKeycode::ESCAPE == 27U);
  STATIC_REQUIRE(DesktopPlatformKeycode::ESCAPE == 27U);
}

TEST_CASE("DesktopPlatformKeycode::KEY_RETURN matches SDL3 SDLK_RETURN",
          "[platform][client][keycode]") {
  // Req: docs/platform/REQUIREMENTS.md §2.3 — Keycode constants
  REQUIRE(DesktopPlatformKeycode::KEY_RETURN == 13U);
  STATIC_REQUIRE(DesktopPlatformKeycode::KEY_RETURN == 13U);
}
