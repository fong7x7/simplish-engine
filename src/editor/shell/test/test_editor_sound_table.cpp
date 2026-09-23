#include <catch2/catch_test_macros.hpp>
#include <editor/project/project-text-file.h>
#include <editor/shell/editor-sound-table.h>
#include <filesystem>
#include <string>

using namespace eng::editor;
namespace fs = std::filesystem;

namespace {

/// A sounds table with @p entries as its rows.
std::string tableOf(const std::string& entries) {
  return R"({"schema": "simplish/data_table/1.0", "id": "sounds",
             "content": {"entry_schema": "simplish/sound/1.0",
                         "entries": )" +
         entries + "}}";
}

}  // namespace

TEST_CASE("a sounds table gives each slot its file") {
  const EditorSoundTable table = parseEditorSoundTable(tableOf(
      R"([{"id": "combat.blast", "file": "sounds/boom.wav"},
          {"id": "combat.shot_fired", "file": "gun.ogg"}])"));
  REQUIRE(table.problems.empty());
  REQUIRE(table.sounds.size() == 2);
  REQUIRE(table.sounds[0].slot == "combat.blast");
  REQUIRE(table.sounds[0].file == fs::path("sounds/boom.wav"));
}

TEST_CASE("rows with nothing to play, or a slot twice, are skipped and said") {
  const EditorSoundTable table = parseEditorSoundTable(tableOf(
      R"([{"id": "combat.blast"}, {"file": "a.wav"}, 7,
          {"id": "combat.blast", "file": "a.wav"},
          {"id": "combat.blast", "file": "b.wav"}])"));
  REQUIRE(table.sounds.size() == 1);
  REQUIRE(table.sounds[0].file == fs::path("a.wav"));
  REQUIRE(table.problems.size() == 4);
}

TEST_CASE("a file that is not a sounds table has no sounds and one problem") {
  REQUIRE(parseEditorSoundTable("{}").problems.size() == 1);
  REQUIRE(parseEditorSoundTable("not json").sounds.empty());
  REQUIRE(parseEditorSoundTable(
              R"({"schema": "simplish/data_table/1.0",
                  "content": {"entry_schema": "simplish/behavior/1.0"}})")
              .problems.size() == 1);
}

TEST_CASE("a table written is read back the same") {
  EditorSoundTable table;
  table.sounds = {{"combat.blast", "sounds/boom.wav"},
                  {"combat.shot_hit_wall", "tick.ogg"}};
  const EditorSoundTable read =
      parseEditorSoundTable(writeEditorSoundTable(table));
  REQUIRE(read.problems.empty());
  REQUIRE(read.sounds.size() == 2);
  REQUIRE(read.sounds[1].slot == "combat.shot_hit_wall");
  REQUIRE(read.sounds[1].file == fs::path("tick.ogg"));
}

TEST_CASE("a project keeps its sounds table beside its other tables") {
  const fs::path root = fs::temp_directory_path() / "simplish-sound-table";
  fs::remove_all(root);
  REQUIRE(loadEditorSoundTable(root).sounds.empty());
  REQUIRE(loadEditorSoundTable(root).problems.empty());
  EditorSoundTable table;
  table.sounds = {{"combat.blast", "boom.wav"}};
  REQUIRE(saveEditorSoundTable(root, table));
  REQUIRE(editorSoundTablePath(root) ==
          root / "content" / "data" / "sounds.data.json");
  REQUIRE(loadEditorSoundTable(root).sounds[0].file == fs::path("boom.wav"));
  fs::remove_all(root);
}
