#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <engine/input/input-bindings-json.h>

using namespace eng::input;

namespace {

/// A stand-in for the platform's key names.
constexpr std::array<KeyName, 2> KEYS = {
    {{"up", 0x40000052U}, {"space", 0x20U}}};

/// A scheme with one of each kind of control in it.
InputBindings mixedScheme() {
  InputBindings bindings = defaultGamepadBindings();
  bindings.bind(InputAction::MOVE_UP, InputSource::key('w'));
  bindings.bind(InputAction::MOVE_UP, InputSource::key(0x40000052U));
  bindings.bind(InputAction::FIRE, InputSource::key(0x20U));
  bindings.bind(InputAction::FIRE, InputSource::key(0x40000099U));
  bindings.setDeadzones({0.25F, 0.125F, 0.5F});
  return bindings;
}

/// Whether @p a and @p b bind the same controls to every action.
bool sameScheme(const InputBindings& a, const InputBindings& b) {
  for (std::size_t i = 0; i < INPUT_ACTION_COUNT; ++i) {
    const auto action = static_cast<InputAction>(i);
    if (!std::ranges::equal(a.sources(action), b.sources(action))) {
      return false;
    }
  }
  return a.deadzones().left_stick == b.deadzones().left_stick &&
         a.deadzones().trigger == b.deadzones().trigger;
}

}  // namespace

TEST_CASE("a scheme survives being written and read back") {
  const InputBindings scheme = mixedScheme();
  const std::string json = writeInputBindings(scheme, KEYS);
  const InputBindingsLoad load = parseInputBindings(json, {}, KEYS);
  REQUIRE(load.problems.empty());
  REQUIRE(sameScheme(load.bindings, scheme));
}

TEST_CASE("controls are written as words a player can edit") {
  const std::string json = writeInputBindings(mixedScheme(), KEYS);
  REQUIRE(json.contains("\"key:w\""));
  REQUIRE(json.contains("\"key:up\""));
  REQUIRE(json.contains("\"key:space\""));
  REQUIRE(json.contains("\"key:0x40000099\""));
  REQUIRE(json.contains("\"pad:-left_y\""));
  REQUIRE(json.contains("\"pad:dpad_up\""));
  REQUIRE(json.contains("\"pad:right_trigger\""));
}

TEST_CASE("an action the file lists gets exactly what it lists") {
  const auto load = parseInputBindings(
      R"({"actions": {"fire": ["pad:south", "pad:+left_trigger"]}})",
      defaultGamepadBindings(), KEYS);
  REQUIRE(load.problems.empty());
  const auto fire = load.bindings.sources(InputAction::FIRE);
  REQUIRE(fire.size() == 2);
  REQUIRE(fire[0] == InputSource::button(GamepadButton::SOUTH));
  REQUIRE(fire[1] == InputSource::positive(GamepadAxis::LEFT_TRIGGER));
  // Left out of the file, so still the default.
  REQUIRE(load.bindings.sources(InputAction::MOVE_UP).size() == 2);
}

TEST_CASE("an empty list unbinds an action") {
  const auto load = parseInputBindings(R"({"actions": {"fire": []}})",
                                       defaultGamepadBindings(), KEYS);
  REQUIRE(load.bindings.sources(InputAction::FIRE).empty());
}

TEST_CASE("unreadable entries are skipped and reported, not fatal") {
  const auto load = parseInputBindings(
      R"({"version": 1, "actions": {"fire": ["pad:south", "pad:banana", 7,
          "key:"], "jump": ["key:j"]}, "deadzones": {"trigger": 0.3}})",
      defaultGamepadBindings(), KEYS);
  REQUIRE(load.problems.size() == 4);
  REQUIRE(load.bindings.sources(InputAction::FIRE).size() == 1);
  REQUIRE(load.bindings.deadzones().trigger == 0.3F);
  REQUIRE(load.bindings.deadzones().left_stick == 0.2F);
}

TEST_CASE("text that is not JSON gives the defaults and one problem") {
  const auto load =
      parseInputBindings("{not json", defaultGamepadBindings(), KEYS);
  REQUIRE(load.problems.size() == 1);
  REQUIRE(sameScheme(load.bindings, defaultGamepadBindings()));
}

TEST_CASE("a key is read by its name, its character, or its hex symbol") {
  const auto load = parseInputBindings(
      R"({"actions": {"fire": ["key:space", "key:q", "key:0x4000003A"]}})", {},
      KEYS);
  REQUIRE(load.problems.empty());
  const auto fire = load.bindings.sources(InputAction::FIRE);
  REQUIRE(fire.size() == 3);
  REQUIRE(fire[0] == InputSource::key(0x20U));
  REQUIRE(fire[1] == InputSource::key('q'));
  REQUIRE(fire[2] == InputSource::key(0x4000003AU));
}
