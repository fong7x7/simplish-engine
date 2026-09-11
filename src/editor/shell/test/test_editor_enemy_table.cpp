#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-enemy-table.h>
#include <filesystem>
#include <string>

using namespace eng;
using namespace eng::editor;

namespace {

/// An enemies table whose entries are @p entries, a JSON array.
std::string table(const std::string& entries) {
  return R"({"schema": "simplish/data_table/1.0", "id": "enemies",
             "content": {"entry_schema": "simplish/enemy/1.0",
                         "entries": )" +
         entries + "}}";
}

/// Whether @p problems has a line containing @p text.
bool mentions(const std::vector<std::string>& problems,
              const std::string& text) {
  for (const std::string& problem : problems) {
    if (problem.find(text) != std::string::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("an enemies table reads every part of an archetype") {
  const EditorEnemyTable read = parseEditorEnemyTable(table(R"([
    {"id": "bloater", "name": "Bloater", "model": "mesh:enemies_bloater",
     "health": 4, "radius": 0.6, "height": 1.2,
     "behavior": "behavior:chase", "faction": "hostile"},
    {"id": "swarmer", "behavior": "chase"}])"));

  REQUIRE(read.problems.empty());
  REQUIRE(read.enemies.size() == 2);
  const game::EnemyDefinition& bloater = read.enemies[0];
  REQUIRE(bloater.name == "Bloater");
  REQUIRE(bloater.model == "mesh:enemies_bloater");
  REQUIRE(bloater.health == 4);
  REQUIRE(bloater.radius == 0.6F);
  REQUIRE(bloater.height == 1.2F);
  REQUIRE(bloater.behavior == "chase");
  // A row that says nothing of its body is an actor's size, named by id.
  REQUIRE(read.enemies[1].name == "swarmer");
  REQUIRE(read.enemies[1].health == game::ENEMY_DEFAULT_HEALTH);
  REQUIRE(read.enemies[1].radius == game::ENEMY_DEFAULT_RADIUS_TILES);
}

TEST_CASE("an archetype's odd values fall back, and are said") {
  const EditorEnemyTable read = parseEditorEnemyTable(table(R"([
    {"id": "odd", "health": "lots", "radius": 9, "faction": "mauve"},
    {"id": "odd"}, {"id": "Not An Id"}])"));

  REQUIRE(read.enemies.size() == 1);
  const game::EnemyDefinition& odd = read.enemies[0];
  REQUIRE(odd.health == game::ENEMY_DEFAULT_HEALTH);
  REQUIRE(odd.radius == EDITOR_ENEMY_MAX_RADIUS);
  REQUIRE(odd.faction == game::Faction::HOSTILE);
  REQUIRE(odd.behavior == "idle");
  REQUIRE(mentions(read.problems, "health is not a number"));
  REQUIRE(mentions(read.problems, "radius was held"));
  REQUIRE(mentions(read.problems, "\"mauve\" is not a faction"));
  REQUIRE(mentions(read.problems, "no behavior, so it stands idle"));
  REQUIRE(mentions(read.problems, "a second row"));
  REQUIRE(mentions(read.problems, "not an id"));
}

TEST_CASE("a file that is not an enemies table gives nothing, and says so") {
  const EditorEnemyTable read = parseEditorEnemyTable(R"({"schema": "x"})");
  REQUIRE(read.enemies.empty());
  REQUIRE(mentions(read.problems, "not an enemies table"));
}

TEST_CASE("a project with no enemies table has none, and no problems") {
  const EditorEnemyTable read =
      loadEditorEnemyTable(std::filesystem::temp_directory_path() /
                           "simplish-no-such-project-for-enemies");
  REQUIRE(read.enemies.empty());
  REQUIRE(read.problems.empty());
  REQUIRE(editorEnemyTablePath("/p").generic_string() ==
          "/p/content/data/enemies.data.json");
}

TEST_CASE("an archetype may go off in a blast when it dies") {
  const EditorEnemyTable read = parseEditorEnemyTable(table(R"([
    {"id": "bloater", "behavior": "bloater", "death_blast_radius": 2.5,
     "death_blast_damage": 3},
    {"id": "swarmer", "behavior": "chase"}])"));
  REQUIRE(read.problems.empty());
  REQUIRE(read.enemies[0].death_blast_radius == 2.5F);
  REQUIRE(read.enemies[0].death_blast_damage == 3);
  REQUIRE(read.enemies[1].death_blast_radius == 0.0F);
}
