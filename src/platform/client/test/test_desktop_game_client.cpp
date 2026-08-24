#include <catch2/catch_test_macros.hpp>
#include <engine/client/desktop-game-client.h>

using namespace eng::client;

namespace {

class TestableDesktopGameClient final : public DesktopGameClient {
public:
  using DesktopGameClient::rhiDevice;

protected:
  bool onTick(float /*dt*/) override { return false; }
};

}  // namespace

// Req: docs/platform/REQUIREMENTS.md §2.3 — DesktopGameClient extraction

TEST_CASE("DesktopGameClient default-constructs without crashing",
          "[platform][client]") {
  // Req: docs/platform/REQUIREMENTS.md §2.3 — Client lifecycle
  TestableDesktopGameClient client;
  REQUIRE(client.rhiDevice() == nullptr);
}

TEST_CASE("DesktopGameClient inherits from RenderedGameClient",
          "[platform][client]") {
  // Req: docs/platform/REQUIREMENTS.md §2.3 — Class hierarchy preserved
  static_assert(
      std::is_base_of_v<eng::client::RenderedGameClient, DesktopGameClient>,
      "DesktopGameClient must inherit from RenderedGameClient");
  static_assert(std::is_base_of_v<eng::client::GameClient, DesktopGameClient>,
                "DesktopGameClient must transitively inherit from GameClient");
  SUCCEED();
}

TEST_CASE("DesktopGameClient::ClientKeyDownKind has expected values",
          "[platform][client]") {
  // Req: docs/platform/REQUIREMENTS.md §2.3 — Enum preserved
  auto first = DesktopGameClient::ClientKeyDownKind::FIRST_PRESS;
  auto repeat = DesktopGameClient::ClientKeyDownKind::REPEAT;
  REQUIRE(first != repeat);
  REQUIRE(static_cast<uint8_t>(first) == 0);
  REQUIRE(static_cast<uint8_t>(repeat) == 1);
}

TEST_CASE(
    "DesktopGameClient::init requires SDL — returns error without display",
    "[platform][client][integration]") {
  // Req: docs/platform/REQUIREMENTS.md §2.3 — Init error handling
  // Note: init() requires SDL3 + a display server.  In headless CI the SDL
  // window creation fails, so init() must return an error string (not crash).
  TestableDesktopGameClient client;
  eng::client::GameClientConfig config;
  auto result = client.init(config);
  // In headless (no display): expect an error string.
  // With a display: init may succeed — both outcomes are valid.
  if (result.has_value()) {
    REQUIRE_FALSE(result->empty());
  } else {
    REQUIRE(client.rhiDevice() != nullptr);
    client.shutdown();
  }
}
