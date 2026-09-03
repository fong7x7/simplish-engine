#include <catch2/catch_test_macros.hpp>
#include <engine/client/desktop-platform-mouse-button.h>

using namespace eng::client;

// Req: docs/platform/REQUIREMENTS.md §2.3 — SDL input maps to GUI events

TEST_CASE("the mirrored SDL button indices match SDL3",
          "[platform][client][mouse]") {
  // desktop-game-client.cpp static_asserts these against the SDL headers;
  // these are the values that file is checking.
  STATIC_REQUIRE(DesktopPlatformMouseButton::LEFT == 1U);
  STATIC_REQUIRE(DesktopPlatformMouseButton::MIDDLE == 2U);
  STATIC_REQUIRE(DesktopPlatformMouseButton::RIGHT == 3U);
}

TEST_CASE("each SDL button maps to its own GUI button",
          "[platform][client][mouse]") {
  // Leaving this unmapped reported every press as LEFT, so middle-drag and
  // right-click were indistinguishable from a left click.
  STATIC_REQUIRE(mapDesktopMouseButton(DesktopPlatformMouseButton::LEFT) ==
                 eng::GuiMouseButton::LEFT);
  STATIC_REQUIRE(mapDesktopMouseButton(DesktopPlatformMouseButton::MIDDLE) ==
                 eng::GuiMouseButton::MIDDLE);
  STATIC_REQUIRE(mapDesktopMouseButton(DesktopPlatformMouseButton::RIGHT) ==
                 eng::GuiMouseButton::RIGHT);
}

TEST_CASE("an unrecognised button reports as left",
          "[platform][client][mouse]") {
  // The extra buttons on a gaming mouse: left is the one every widget
  // already handles.
  STATIC_REQUIRE(mapDesktopMouseButton(0U) == eng::GuiMouseButton::LEFT);
  STATIC_REQUIRE(mapDesktopMouseButton(4U) == eng::GuiMouseButton::LEFT);
  STATIC_REQUIRE(mapDesktopMouseButton(255U) == eng::GuiMouseButton::LEFT);
}
