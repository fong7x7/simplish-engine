#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-graphics-file.h>
#include <filesystem>
#include <fstream>

using namespace eng;
using namespace eng::editor;
namespace fs = std::filesystem;

TEST_CASE("graphics settings survive being written and read back") {
  for (const WaterFidelity fidelity : WATER_FIDELITIES) {
    EditorGraphicsSettings settings{};
    settings.water = fidelity;
    std::vector<std::string> problems;
    const EditorGraphicsSettings read =
        parseEditorGraphics(writeEditorGraphics(settings), problems);
    CHECK(problems.empty());
    CHECK(read.water == fidelity);
  }
}

TEST_CASE("a graphics file that gets the water wrong keeps the default") {
  std::vector<std::string> problems;
  CHECK(parseEditorGraphics(R"({"water": "ultra"})", problems).water ==
        WATER_DEFAULT_FIDELITY);
  CHECK(problems.size() == 1);
  problems.clear();
  CHECK(parseEditorGraphics("not json", problems).water ==
        WATER_DEFAULT_FIDELITY);
  CHECK(problems.size() == 1);
  problems.clear();
  CHECK(parseEditorGraphics("{}", problems).water == WATER_DEFAULT_FIDELITY);
  CHECK(problems.empty());
}

TEST_CASE("a missing graphics file is written with the defaults") {
  const fs::path file =
      fs::temp_directory_path() / "simplish-graphics" / "graphics.json";
  fs::remove_all(file.parent_path());
  const EditorGraphicsSettings loaded = loadEditorGraphics(file);
  CHECK(loaded.water == WATER_DEFAULT_FIDELITY);
  CHECK(loaded.file == file);
  CHECK(fs::exists(file));
  fs::remove_all(file.parent_path());
}

TEST_CASE("a saved graphics file is what the next load reads") {
  const fs::path file = fs::temp_directory_path() / "simplish-graphics-2.json";
  EditorGraphicsSettings settings{};
  settings.water = WaterFidelity::LOW;
  settings.file = file;
  REQUIRE(saveEditorGraphics(settings));
  CHECK(loadEditorGraphics(file).water == WaterFidelity::LOW);
  fs::remove(file);
  CHECK_FALSE(saveEditorGraphics({}));
}

// Req: docs/engine/water.md §5 — the water's costlier effects are the
// user's to switch off, kept with their graphics settings.
TEST_CASE("the water's effects survive being written and read back") {
  EditorGraphicsSettings settings{};
  settings.water_effects.on[waterEffectIndex(WaterEffect::REFLECTIONS)] = false;
  settings.water_effects.on[waterEffectIndex(WaterEffect::CAUSTICS)] = false;
  std::vector<std::string> problems;
  const EditorGraphicsSettings read =
      parseEditorGraphics(writeEditorGraphics(settings), problems);
  CHECK(problems.empty());
  CHECK(read.water_effects == settings.water_effects);
  CHECK(parseEditorGraphics(R"({"water": "low"})", problems).water_effects ==
        WaterEffects{});
}

TEST_CASE("a graphics file that gets an effect wrong keeps the rest") {
  std::vector<std::string> problems;
  const WaterEffects read =
      parseEditorGraphics(
          R"({"water_effects": {"reflections": "no", "contact": false,
                                "sparkle": true}})",
          problems)
          .water_effects;
  CHECK(problems.size() == 2);
  CHECK(waterEffectOn(read, WaterEffect::REFLECTIONS));
  CHECK_FALSE(waterEffectOn(read, WaterEffect::CONTACT));
  problems.clear();
  CHECK(
      parseEditorGraphics(R"({"water_effects": 3})", problems).water_effects ==
      WaterEffects{});
  CHECK(problems.size() == 1);
}

TEST_CASE("the interface scale survives being written and read back") {
  EditorGraphicsSettings settings;
  settings.ui_scale = 1.25f;
  std::vector<std::string> problems;
  const EditorGraphicsSettings read =
      parseEditorGraphics(writeEditorGraphics(settings), problems);
  CHECK(problems.empty());
  CHECK(read.ui_scale == 1.25f);
}

TEST_CASE("a graphics file with an interface scale out of range keeps 1") {
  std::vector<std::string> problems;
  const EditorGraphicsSettings read =
      parseEditorGraphics(R"({"interface_scale": 9})", problems);
  CHECK(read.ui_scale == 1.0f);
  REQUIRE(problems.size() == 1);
  CHECK(problems.front() == "interface_scale must be a number from 0.5 to 3");
}
