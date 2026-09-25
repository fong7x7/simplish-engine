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
#include <editor/build/editor-deploy-manifest.h>
#include <editor/build/editor-setup-json.h>
#include <editor/project/project-paths.h>
#include <editor/project/project-text-file.h>
#include <engine/sim/simulation.h>
#include <game/logic/game-logic-instance.h>
#include <game/world/game-world.h>
#include <thread>

using namespace eng::editor;

namespace {

/// The editor's own toolchain, pointed at this checkout.
EditorToolchain testTools() {
  EditorToolchain tools = editorToolchain();
  tools.engine_root = SIMPLISH_TEST_ENGINE_ROOT;
  return tools;
}

/// Build the logic of the project at @p root with the editor's own
/// toolchain — and check it, when @p commands says so — and say how it
/// went.
EditorBuildStatus buildLogic(const std::filesystem::path& root,
                             std::vector<EditorBuildCommand> commands) {
  EditorBuildJob job;
  const auto log = projectBuildLogPath(root, EditorBuildKind::LOGIC);
  (void)job.start(std::move(commands), log);
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
  setup.actor_capacity = eng::game::GAME_LOGIC_ACTOR_CAPACITY;
  eng::game::ActorSpawn actor;
  actor.at = {30.5F, 30.5F, 0.0F};
  actor.behavior = "idle";
  setup.actors.push_back(actor);
  return setup;
}

/// Play a run of setup() with @p library's logic until it is over, and
/// give back everything the logic said.
std::vector<std::string> playToTheEnd(const EditorLogicLibrary& library) {
  const eng::game::GameLogicInstance logic(library.factory());
  eng::game::GameWorld world(setup(), {}, logic.get());
  eng::sim::Simulation simulation(world, eng::sim::TickHashing::ON);
  while (!world.runOver() && simulation.nextTick() < 100000) {
    (void)simulation.step({});
  }
  REQUIRE(world.runOver());
  return world.takeLogicLog();
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
  REQUIRE(buildLogic(dir.path(), logicBuildCommands(dir.path(), testTools())) ==
          EditorBuildStatus::SUCCEEDED);
  REQUIRE_FALSE(projectLogicStale(dir.path()));

  const EditorLogicLibraryLoad load = loadEditorLogicLibrary(
      projectLogicLibraryPath(dir.path()), projectLogicLoadPath(dir.path(), 1));
  REQUIRE(load.error.empty());
  // Nobody holds the controls, so the waves win: the run ends either way.
  const std::vector<std::string> log = playToTheEnd(*load.library);
  REQUIRE(log.size() >= 2);
  REQUIRE(log[0] == "Survive 90 s");
  REQUIRE(log[1].starts_with("Wave 1: "));
}

namespace {

/// A logic that crashes on its fifth tick.
constexpr std::string_view CRASHING_LOGIC = R"(
#include <game/logic/game-logic-entry.h>
#include <game/logic/game-logic.h>
namespace {
class Crashes final : public eng::game::GameLogic {
public:
  void tick(eng::game::GameLogicWorld& world) override {
    if (world.tick() == 5) {
      volatile int* nowhere = nullptr;
      *nowhere = 1;
    }
  }
};
}  // namespace
SIMPLISH_GAME_LOGIC(Crashes)
)";

/// Lay the check's content out for the project at @p root: setup() as its
/// one level.
void bakeCheck(const std::filesystem::path& root) {
  const auto check = projectLogicCheckPath(root);
  REQUIRE(writeProjectTextFile(check / "levels" / "check.setup.json",
                               serializeGameSetup(setup())));
  REQUIRE(writeProjectTextFile(
      check / EDITOR_DEPLOY_MANIFEST,
      serializeDeployManifest({"check", {"check"}, "check", true})));
}

/// The logic build and its check, for the project at @p root.
std::vector<EditorBuildCommand> checkedBuild(const std::filesystem::path& root) {
  std::vector<EditorBuildCommand> commands =
      logicBuildCommands(root, testTools());
  commands.push_back(logicCheckCommand(root, testTools()).value());
  return commands;
}

}  // namespace

TEST_CASE("a logic that crashes fails its check, and the build with it",
          "[toolchain]") {
  const test::BuildTempDir dir("crashing-logic");
  REQUIRE(scaffoldProjectLogic(dir.path()) == EditorLogicScaffold::CREATED);
  REQUIRE(writeProjectTextFile(projectSourcePath(dir.path()) /
                                   LOGIC_EXAMPLE_FILE_NAME,
                               CRASHING_LOGIC));
  bakeCheck(dir.path());

  REQUIRE(buildLogic(dir.path(), checkedBuild(dir.path())) ==
          EditorBuildStatus::FAILED);
  const auto lines =
      readLogLines(projectBuildLogPath(dir.path(), EditorBuildKind::LOGIC));
  REQUIRE(buildErrorLines(lines, 5).back().find("crashed") !=
          std::string::npos);
}

TEST_CASE("a logic that runs cleanly passes its check", "[toolchain]") {
  const test::BuildTempDir dir("checked-logic");
  REQUIRE(scaffoldProjectLogic(dir.path()) == EditorLogicScaffold::CREATED);
  bakeCheck(dir.path());

  REQUIRE(buildLogic(dir.path(), checkedBuild(dir.path())) ==
          EditorBuildStatus::SUCCEEDED);
}
