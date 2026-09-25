#include "support/build-temp-dir.h"
#include <catch2/catch_test_macros.hpp>
#include <editor/build/editor-logic-source.h>
#include <editor/project/project-paths.h>
#include <editor/project/project-text-file.h>

using namespace eng::editor;

TEST_CASE("a project with no src has no game logic until it is scaffolded") {
  const test::BuildTempDir dir("scaffold");

  REQUIRE_FALSE(projectHasLogic(dir.path()));
  REQUIRE(scaffoldProjectLogic(dir.path()) == EditorLogicScaffold::CREATED);
  REQUIRE(projectHasLogic(dir.path()));
  REQUIRE(std::filesystem::exists(projectSourcePath(dir.path()) /
                                  LOGIC_EXAMPLE_FILE_NAME));
  REQUIRE(std::filesystem::exists(projectBuildPath(dir.path()) /
                                  ".gitignore"));
}

TEST_CASE("scaffolding never writes over a project's own logic") {
  const test::BuildTempDir dir("scaffold-kept");
  const auto cmake = projectSourcePath(dir.path()) / LOGIC_CMAKE_FILE_NAME;
  REQUIRE(writeProjectTextFile(cmake, "# mine\n"));

  REQUIRE(scaffoldProjectLogic(dir.path()) ==
          EditorLogicScaffold::ALREADY_THERE);
  REQUIRE(readProjectTextFile(cmake) == "# mine\n");
}

TEST_CASE("logic is stale when its source has no library built from it") {
  const test::BuildTempDir dir("stale");

  REQUIRE_FALSE(projectLogicStale(dir.path()));
  REQUIRE(scaffoldProjectLogic(dir.path()) == EditorLogicScaffold::CREATED);
  REQUIRE(projectLogicStale(dir.path()));
}

TEST_CASE("the scaffold's example exports its logic") {
  REQUIRE(logicScaffoldSource().find("SIMPLISH_GAME_LOGIC(") !=
          std::string::npos);
  REQUIRE(logicScaffoldCMake().find("simplish_game_logic(") !=
          std::string::npos);
}
