#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-character-table.h>
#include <filesystem>
#include <fstream>
#include <string>

using namespace eng::editor;

namespace {

/// A characters table holding @p entries, a JSON array's contents.
std::string table(const std::string& entries) {
  return R"({"schema": "simplish/data_table/1.0", "id": "characters",
             "name": "Characters",
             "content": {"entry_schema": "simplish/character/1.0",
                         "entries": [)" +
         entries + "]}}";
}

}  // namespace

TEST_CASE("a characters table reads every row, in order") {
  const EditorCharacterTable read = parseEditorCharacterTable(table(R"(
      {"id": "scout", "name": "Scout", "model": "mesh:scout",
       "move_speed": 7.5, "health": 3},
      {"id": "tank", "name": "Tank", "move_speed": 3, "health": 9})"));

  REQUIRE(read.problems.empty());
  REQUIRE(read.characters.size() == 2);
  REQUIRE(read.characters[0].id == "scout");
  REQUIRE(read.characters[0].model == "mesh:scout");
  REQUIRE(read.characters[0].move_speed == 7.5f);
  REQUIRE(read.characters[1].health == 9);
  REQUIRE(read.characters[1].model.empty());
}

TEST_CASE("a row with no stats takes the default character's") {
  const EditorCharacterTable read =
      parseEditorCharacterTable(table(R"({"id": "plain"})"));

  REQUIRE(read.problems.empty());
  REQUIRE(read.characters[0].name == "plain");
  REQUIRE(read.characters[0].move_speed ==
          eng::game::DEFAULT_CHARACTER_MOVE_SPEED);
  REQUIRE(read.characters[0].health == eng::game::DEFAULT_CHARACTER_HEALTH);
}

TEST_CASE("bad rows are skipped and bad stats held, each said so") {
  const EditorCharacterTable read = parseEditorCharacterTable(table(R"(
      {"name": "No id"},
      {"id": "Not An Id"},
      {"id": "fast", "move_speed": 500, "health": 0},
      {"id": "fast"},
      {"id": "odd", "move_speed": "quick"})"));

  REQUIRE(read.characters.size() == 2);
  REQUIRE(read.characters[0].move_speed == EDITOR_CHARACTER_MAX_SPEED);
  REQUIRE(read.characters[0].health == 1);
  REQUIRE(read.characters[1].move_speed ==
          eng::game::DEFAULT_CHARACTER_MOVE_SPEED);
  // Two skipped for their ids, one repeated, two stats held, one not a
  // number.
  REQUIRE(read.problems.size() == 6);
}

TEST_CASE("a file that is not a characters table has no characters") {
  REQUIRE(parseEditorCharacterTable("{ not json").problems.size() == 1);
  const EditorCharacterTable weapons = parseEditorCharacterTable(
      R"({"schema": "simplish/data_table/1.0",
          "content": {"entry_schema": "simplish/weapon/1.0",
                      "entries": [{"id": "rifle"}]}})");
  REQUIRE(weapons.characters.empty());
  REQUIRE(weapons.problems.size() == 1);
}

TEST_CASE("a project with no table has no characters and no problems") {
  const auto root =
      std::filesystem::temp_directory_path() / "simplish-character-table-test";
  std::filesystem::remove_all(root);
  REQUIRE(loadEditorCharacterTable(root).characters.empty());
  REQUIRE(loadEditorCharacterTable(root).problems.empty());

  const auto path = editorCharacterTablePath(root);
  REQUIRE(path == root / "content" / "data" / "characters.data.json");
  std::filesystem::create_directories(path.parent_path());
  std::ofstream(path) << table(R"({"id": "scout"})");
  REQUIRE(loadEditorCharacterTable(root).characters.size() == 1);
  std::filesystem::remove_all(root);
}
