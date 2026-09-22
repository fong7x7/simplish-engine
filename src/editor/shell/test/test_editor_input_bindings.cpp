#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-input-bindings.h>
#include <filesystem>
#include <fstream>

using namespace eng;
using namespace eng::editor;

namespace {

/// A fresh scratch directory for one test.
std::filesystem::path scratchDir(const char* name) {
  const auto dir = std::filesystem::temp_directory_path() /
                   "simplish-editor-input-bindings" / name;
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

}  // namespace

TEST_CASE("with no file yet, the defaults are played and written out") {
  const auto file = scratchDir("fresh") / "controls" / "input-bindings.json";
  const input::InputBindings bindings = loadEditorInputBindings(file);
  REQUIRE(bindings.actionsFor(input::InputSource::key('w')) ==
          std::vector{input::InputAction::MOVE_UP});
  REQUIRE(std::filesystem::exists(file));
  // What was written reads back as the same scheme.
  const input::InputBindings again = loadEditorInputBindings(file);
  REQUIRE(again.sources(input::InputAction::FIRE).size() ==
          bindings.sources(input::InputAction::FIRE).size());
}

TEST_CASE("the user's file remaps what it lists and keeps the rest") {
  const auto file = scratchDir("remapped") / "input-bindings.json";
  std::ofstream(file) << R"({"actions": {"fire": ["pad:south", "key:space"]}})";
  const input::InputBindings bindings = loadEditorInputBindings(file);
  const auto fire = bindings.sources(input::InputAction::FIRE);
  REQUIRE(fire.size() == 2);
  REQUIRE(fire[0] == input::InputSource::button(input::GamepadButton::SOUTH));
  REQUIRE(fire[1] == input::InputSource::key(' '));
  REQUIRE_FALSE(bindings.sources(input::InputAction::MOVE_UP).empty());
}

TEST_CASE("no path plays the defaults and writes nothing") {
  const input::InputBindings bindings = loadEditorInputBindings({});
  REQUIRE_FALSE(bindings.sources(input::InputAction::MOVE_UP).empty());
}
