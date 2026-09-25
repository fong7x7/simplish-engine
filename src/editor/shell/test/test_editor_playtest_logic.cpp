#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-playtest-session.h>
#include <memory>
#include <vector>

using namespace eng;
using namespace eng::editor;

namespace {

/// Wins on tick 2, saying so.
class WinsOnTwo final : public game::GameLogic {
public:
  void tick(game::GameLogicWorld& world) override {
    if (world.tick() == 2) {
      world.log("won on two");
      world.endRun(game::RunOutcome::WON);
    }
  }
};

game::GameLogic* makeLogic() {
  return std::make_unique<WinsOnTwo>().release();
}

/// Spawns one actor, `imp`, on tick 0.
class SpawnsImp final : public game::GameLogic {
public:
  void tick(game::GameLogicWorld& world) override {
    if (world.tick() == 0) {
      (void)world.spawnActor({.at = {4.5F, 4.5F, 0.0F},
                              .behavior = "wander",
                              .id = "imp",
                              .model = "mesh:imp"});
    }
  }
};

game::GameLogic* makeSpawner() {
  return std::make_unique<SpawnsImp>().release();
}

/// Cues a blast's sound with smoke on tick 0, and an effect there is none
/// of on tick 1.
class Cues final : public game::GameLogic {
public:
  void tick(game::GameLogicWorld& world) override {
    if (world.tick() == 0) {
      world.cue({.at = {2.0F, 2.0F, 0.0F},
                 .sound = "combat.blast",
                 .effect = "smoke"});
    } else if (world.tick() == 1) {
      world.cue({.effect = "confetti"});
    }
  }
};

game::GameLogic* makeCues() {
  return std::make_unique<Cues>().release();
}

void unmakeLogic(game::GameLogic* logic) {
  const std::unique_ptr<game::GameLogic> owned(logic);
}

/// A "library" holding the logic above: nothing to open or close, since
/// the logic is linked into this test.
std::shared_ptr<EditorLogicLibrary> linkedLibrary() {
  return std::make_shared<EditorLogicLibrary>(
      nullptr, game::GameLogicFactory{makeLogic, unmakeLogic},
      std::filesystem::path{});
}

/// Step @p session @p ticks times on no input.
void run(EditorPlaytestSession& session, int ticks) {
  std::vector<EditorScriptedInput> scripted;
  for (int i = 0; i < ticks; ++i) {
    session.step({}, scripted);
  }
}

}  // namespace

TEST_CASE("a playtest runs the project's game logic and reports it") {
  EditorPlaytestSession session(game::GameSetup{}, {},
                                {"main", linkedLibrary()});
  EditorPlaytestState state;

  run(session, 3);
  session.publish(state);

  CHECK(state.logic);
  CHECK(state.run_over);
  CHECK(state.outcome == game::RunOutcome::WON);
  CHECK(state.logic_log == std::vector<std::string>{"won on two"});
}

TEST_CASE("a playtest keeps its logic's library open while it runs") {
  std::shared_ptr<EditorLogicLibrary> library = linkedLibrary();
  const std::weak_ptr<EditorLogicLibrary> watched = library;
  auto session = std::make_unique<EditorPlaytestSession>(
      game::GameSetup{}, game::GameContent{},
      EditorPlaytestRun{"main", std::move(library)});

  CHECK_FALSE(watched.expired());
  session.reset();
  CHECK(watched.expired());
}

TEST_CASE("a playtest with no library plays without logic") {
  EditorPlaytestSession session(game::GameSetup{}, {}, {"main"});
  EditorPlaytestState state;

  run(session, 3);
  session.publish(state);

  CHECK_FALSE(state.logic);
  CHECK(state.outcome == game::RunOutcome::PLAYING);
}

TEST_CASE("a playtest reports the actors its logic spawns, and how to draw "
          "them") {
  game::GameSetup setup;
  setup.actor_capacity = 4;
  EditorPlaytestSession session(
      setup, {},
      {"main", std::make_shared<EditorLogicLibrary>(
                   nullptr, game::GameLogicFactory{makeSpawner, unmakeLogic},
                   std::filesystem::path{})});
  EditorPlaytestState state;

  run(session, 2);
  session.publish(state);

  REQUIRE(state.actors.size() == 1);
  CHECK(state.actors[0].id == "imp");
  CHECK(state.actors[0].spawned);
  REQUIRE(session.spawnedActors() == std::vector<uint32_t>{0});
  CHECK(session.actorModel(0) == "mesh:imp");
}

TEST_CASE("a playtest shows the effects its logic cues, and keeps their "
          "sounds for the speakers") {
  EditorPlaytestSession session(
      game::GameSetup{}, {},
      {"main", std::make_shared<EditorLogicLibrary>(
                   nullptr, game::GameLogicFactory{makeCues, unmakeLogic},
                   std::filesystem::path{})});
  EditorPlaytestState state;

  run(session, 2);
  session.publish(state);

  REQUIRE(state.logic_cues.size() == 2);
  CHECK(state.logic_cues[0].sound == "combat.blast");
  CHECK(state.logic_cues[1].tick == 1);
  CHECK(session.effects().particles.live > 0);
  const std::vector<game::WorldCue> heard = session.takeLogicCues();
  REQUIRE(heard.size() == 1);
  CHECK(heard[0].at.x == 2.0F);
  CHECK(session.takeLogicCues().empty());
}
