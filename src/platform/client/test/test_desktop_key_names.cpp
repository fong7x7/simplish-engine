#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <engine/client/desktop-key-names.h>
#include <engine/client/desktop-platform-keycode.h>
#include <engine/input/input-bindings-json.h>
#include <set>
#include <string_view>

using eng::client::desktopKeyNames;
using eng::client::DesktopPlatformKeycode;
using namespace eng::input;

TEST_CASE("each key name is unique, and names one key") {
  std::set<std::string_view> names;
  std::set<uint32_t> keys;
  for (const KeyName& entry : desktopKeyNames()) {
    REQUIRE(names.insert(entry.name).second);
    REQUIRE(keys.insert(entry.key).second);
  }
}

TEST_CASE("the arrows are named for the keycodes the playtest reads") {
  const auto keys = desktopKeyNames();
  const auto up = std::ranges::find(keys, "up", &KeyName::name);
  REQUIRE(up != keys.end());
  REQUIRE(up->key == DesktopPlatformKeycode::ARROW_UP);
}

TEST_CASE("a desktop scheme survives the file by name") {
  InputBindings bindings;
  bindings.bind(InputAction::MOVE_LEFT,
                InputSource::key(DesktopPlatformKeycode::ARROW_LEFT));
  bindings.bind(InputAction::FIRE,
                InputSource::key(DesktopPlatformKeycode::F5));
  const std::string json = writeInputBindings(bindings, desktopKeyNames());
  REQUIRE(json.contains("\"key:left\""));
  REQUIRE(json.contains("\"key:f5\""));
  const auto load = parseInputBindings(json, {}, desktopKeyNames());
  REQUIRE(load.problems.empty());
  REQUIRE(load.bindings.actionsFor(
              InputSource::key(DesktopPlatformKeycode::ARROW_LEFT)) ==
          std::vector{InputAction::MOVE_LEFT});
}
