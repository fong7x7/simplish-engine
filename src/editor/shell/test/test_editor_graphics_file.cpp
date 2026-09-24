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
