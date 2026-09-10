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

TEST_CASE("DesktopPlatformKeycode::BACKSPACE matches SDL3 SDLK_BACKSPACE",
          "[platform][client][keycode]") {
  // Req: docs/platform/REQUIREMENTS.md §2.3 — Keycode constants
  REQUIRE(DesktopPlatformKeycode::BACKSPACE == 8U);
  STATIC_REQUIRE(DesktopPlatformKeycode::BACKSPACE == 8U);
}

TEST_CASE("DesktopPlatformKeycode::DELETE_FORWARD matches SDL3 SDLK_DELETE",
          "[platform][client][keycode]") {
  // Req: docs/platform/REQUIREMENTS.md §2.3 — Keycode constants
  REQUIRE(DesktopPlatformKeycode::DELETE_FORWARD == 127U);
  STATIC_REQUIRE(DesktopPlatformKeycode::DELETE_FORWARD == 127U);
}

TEST_CASE("DesktopPlatformKeycode's playtest keys match SDL3's",
          "[platform][client][keycode]") {
  // The values SDL3 gives F5 and the arrow keys: scancode | (1 << 30).
  // desktop-game-client.cpp also static_asserts them against SDL itself.
  STATIC_REQUIRE(DesktopPlatformKeycode::F5 == 0x4000003EU);
  STATIC_REQUIRE(DesktopPlatformKeycode::ARROW_RIGHT == 0x4000004FU);
  STATIC_REQUIRE(DesktopPlatformKeycode::ARROW_LEFT == 0x40000050U);
  STATIC_REQUIRE(DesktopPlatformKeycode::ARROW_DOWN == 0x40000051U);
  STATIC_REQUIRE(DesktopPlatformKeycode::ARROW_UP == 0x40000052U);
}
