#include <catch2/catch_test_macros.hpp>
#include <editor/build/editor-build-paths.h>

using namespace eng::editor;

TEST_CASE("every build writes under the project's build folder") {
  const std::filesystem::path root = "/games/dig";

  REQUIRE(projectLogicBuildPath(root) == "/games/dig/build/logic");
  REQUIRE(projectDeployPath(root) == "/games/dig/build/deploy");
  REQUIRE(projectDeployBuildPath(root) == "/games/dig/build/deploy-cmake");
  REQUIRE(projectBuildLogPath(root, EditorBuildKind::DEPLOY) ==
          "/games/dig/build/deploy.log");
}

TEST_CASE("the logic library is named for this platform") {
  const std::filesystem::path library =
      projectLogicLibraryPath("/games/dig");

  REQUIRE(library.parent_path() == "/games/dig/build/logic");
  REQUIRE(library.stem() == "game-logic");
#ifdef __APPLE__
  REQUIRE(library.extension() == ".dylib");
#endif
}

TEST_CASE("each load of the logic library is a copy of its own") {
  const std::filesystem::path first = projectLogicLoadPath("/g", 1);
  const std::filesystem::path second = projectLogicLoadPath("/g", 2);

  REQUIRE(first != second);
  REQUIRE(first.parent_path() == "/g/build/logic/loaded");
  REQUIRE(first.extension() == projectLogicLibraryPath("/g").extension());
}
