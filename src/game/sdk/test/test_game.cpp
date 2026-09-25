#include "support/sdk-rig.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <game/sdk/game.h>
#include <vector>

using namespace eng::game;
using namespace eng::game::sdk;

namespace {

/// Kills grunt_a on tick 0, and writes down the hooks it hears.
class Listener final : public Game {
public:
  /// The hooks called, in order.
  std::vector<std::string> heard;

protected:
  void onStart(GameLogicWorld& world) override {
    heard.emplace_back("start");
    world.damage(world.actor(0).target, 99);
  }
  void onTick([[maybe_unused]] GameLogicWorld& world) override {
    heard.emplace_back("tick");
  }
  void onActorDied([[maybe_unused]] GameLogicWorld& world,
                   const LogicEvent& event) override {
    heard.emplace_back("died " + std::string(event.id));
  }
};

}  // namespace

TEST_CASE("a Game hears each event of the last tick, then ticks") {
  Listener logic;

  test::runLogic(logic, 2, test::sdkArena(), {});

  CHECK(logic.heard ==
        std::vector<std::string>{"start", "tick", "died grunt_a", "tick"});
}

namespace {

/// Ends the run on tick 100, and writes down the hooks an actor's own
/// doings and the end call.
class Watcher final : public Game {
public:
  /// The hooks called, in order.
  std::vector<std::string> heard;

protected:
  void onTick(GameLogicWorld& world) override {
    if (world.tick() == 100) {
      world.endRun(RunOutcome::WON);
    }
  }
  void onActorNoticed(GameLogicWorld&, const LogicEvent&) override {
    heard.emplace_back("noticed");
  }
  void onActorAttacked(GameLogicWorld&, const LogicEvent&) override {
    heard.emplace_back("attacked");
  }
  void onActorStateEntered(GameLogicWorld&, const LogicEvent& event) override {
    heard.emplace_back("state " + std::string(event.state));
  }
  void onRunEnded(GameLogicWorld& world) override {
    heard.emplace_back(world.outcome() == RunOutcome::WON ? "won" : "lost");
  }
};

/// Whether @p heard holds @p hook.
bool holds(const std::vector<std::string>& heard, std::string_view hook) {
  return std::ranges::find(heard, hook) != heard.end();
}

}  // namespace

TEST_CASE("a Game hears what actors do, and when the run ends") {
  Watcher logic;
  GameSetup setup = test::sdkArena();
  setup.actors[0].behavior = "chase";
  setup.actors[0].at = {2.1F, 1.5F, 0.0F};

  test::runLogic(logic, 120, setup, {});

  CHECK(holds(logic.heard, "noticed"));
  CHECK(holds(logic.heard, "state pursue"));
  CHECK(holds(logic.heard, "attacked"));
  CHECK(logic.heard.back() == "won");
  CHECK(std::ranges::count(logic.heard, std::string("won")) == 1);
}
