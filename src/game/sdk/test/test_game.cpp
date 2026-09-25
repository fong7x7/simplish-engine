#include "support/sdk-rig.h"

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
