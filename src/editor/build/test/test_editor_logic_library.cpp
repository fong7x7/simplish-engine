#include "support/build-temp-dir.h"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <editor/build/editor-build-commands.h>
#include <editor/build/editor-build-job.h>
#include <editor/build/editor-build-log.h>
#include <editor/build/editor-build-paths.h>
#include <editor/build/editor-logic-library-load.h>
#include <editor/build/editor-logic-source.h>
#include <editor/build/editor-toolchain.h>
#include <engine/sim/simulation.h>
#include <game/logic/game-logic-instance.h>
#include <game/world/game-world.h>
#include <thread>

using namespace eng::editor;

namespace {

/// Build the logic of the project at @p root with the editor's own
/// toolchain, and say how it went.
EditorBuildStatus buildLogic(const std::filesystem::path& root) {
  EditorToolchain tools = editorToolchain();
  tools.engine_root = SIMPLISH_TEST_ENGINE_ROOT;
  EditorBuildJob job;
  const auto log = projectBuildLogPath(root, EditorBuildKind::LOGIC);
  (void)job.start(logicBuildCommands(root, tools), log);
  while (job.status() == EditorBuildStatus::RUNNING) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  for (const std::string& line : readLogTail(log, 30)) {
    UNSCOPED_INFO(line);
  }
  return job.status();
}

/// One player, and one hostile actor standing still out of reach.
eng::game::GameSetup setup() {
  eng::game::GameSetup setup;
  eng::game::ActorSpawn actor;
  actor.at = {30.5F, 30.5F, 0.0F};
  actor.behavior = "idle";
  setup.actors.push_back(actor);
  return setup;
}

}  // namespace

TEST_CASE("a library that is not there loads as a reason") {
  const test::BuildTempDir dir("load-missing");

  const EditorLogicLibraryLoad load = loadEditorLogicLibrary(
      dir.path() / "none.dylib", projectLogicLoadPath(dir.path(), 1));

  REQUIRE(load.library == nullptr);
  REQUIRE_FALSE(load.error.empty());
}

TEST_CASE("the scaffold builds with the editor's toolchain, loads, and "
          "plays",
          "[toolchain]") {
  const test::BuildTempDir dir("scaffold-build");
  REQUIRE(scaffoldProjectLogic(dir.path()) == EditorLogicScaffold::CREATED);
  REQUIRE(buildLogic(dir.path()) == EditorBuildStatus::SUCCEEDED);
  REQUIRE_FALSE(projectLogicStale(dir.path()));

  const EditorLogicLibraryLoad load = loadEditorLogicLibrary(
      projectLogicLibraryPath(dir.path()), projectLogicLoadPath(dir.path(), 1));
  REQUIRE(load.error.empty());
  eng::game::GameLogicInstance logic(load.library->factory());
  eng::game::GameWorld world(setup(), {}, logic.get());
  eng::sim::Simulation simulation(world, eng::sim::TickHashing::ON);
  while (!world.runOver() && simulation.nextTick() < 100000) {
    (void)simulation.step({});
  }
  REQUIRE(world.outcome() == eng::game::RunOutcome::WON);
  REQUIRE(world.takeLogicLog().back() == "Survived");
}
