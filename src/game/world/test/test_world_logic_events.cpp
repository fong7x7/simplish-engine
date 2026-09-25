#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <engine/sim/simulation.h>
#include <functional>
#include <game/logic/game-logic.h>
#include <game/player/player-system.h>
#include <game/world/game-world.h>
#include <string>
#include <utility>
#include <vector>

using eng::game::GameLogic;
using eng::game::GameLogicWorld;
using eng::game::GameSetup;
using eng::game::GameWorld;
using eng::game::LogicEvent;
using eng::game::LogicEventKind;
using eng::game::RunOutcome;
using eng::sim::Simulation;
using eng::sim::TickHashing;
using eng::sim::TickInput;

namespace {

/// A logic that keeps every event it hears, and runs whatever a test says
/// each tick first.
class Listening final : public GameLogic {
public:
  explicit Listening(std::function<void(GameLogicWorld&)> each = {})
    : each_(std::move(each)) {}
  void tick(GameLogicWorld& world) override {
    if (each_) {
      each_(world);
    }
    heard_.insert(heard_.end(), world.events().begin(), world.events().end());
    for (LogicEvent& event : heard_) {
      event.id = {};
    }
  }
  void end(GameLogicWorld& world) override {
    ++ends_;
    outcome_ = world.outcome();
    world.log("the run is over");
  }
  /// The first event of @p kind heard, if any; its id and state dropped.
  [[nodiscard]] const LogicEvent* first(LogicEventKind kind) const {
    const auto found = std::ranges::find(heard_, kind, &LogicEvent::kind);
    return found == heard_.end() ? nullptr : &*found;
  }

  /// Every event heard, ids and states dropped: they view the tick's.
  std::vector<LogicEvent> heard_;
  /// How many times `end` was called.
  uint32_t ends_ = 0;
  /// The outcome `end` saw.
  RunOutcome outcome_ = RunOutcome::PLAYING;

private:
  /// The test's tick.
  std::function<void(GameLogicWorld&)> each_;
};

/// Step @p simulation @p ticks times on no input.
void run(Simulation& simulation, uint64_t ticks) {
  for (uint64_t i = 0; i < ticks; ++i) {
    (void)simulation.step(TickInput{});
  }
}

/// One player at (1.5, 1.5) and a hostile chaser a step from them.
GameSetup chaserBeside() {
  GameSetup setup;
  setup.spawns[0] = {1.5F, 1.5F, 0.0F};
  eng::game::ActorSpawn chaser;
  chaser.at = {2.1F, 1.5F, 0.0F};
  chaser.behavior = "chase";
  setup.actors.push_back(chaser);
  setup.actor_capacity = 8;
  return setup;
}

}  // namespace

TEST_CASE("game logic hears an actor notice a player, then attack them") {
  Listening logic;
  GameWorld world(chaserBeside(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  run(simulation, 60);

  const LogicEvent* noticed = logic.first(LogicEventKind::ACTOR_NOTICED);
  const LogicEvent* attacked = logic.first(LogicEventKind::ACTOR_ATTACKED);
  REQUIRE(noticed != nullptr);
  REQUIRE(attacked != nullptr);
  CHECK(noticed < attacked);
  const auto player = world.players().slots.handleAt(0);
  CHECK(noticed->other->index == player.index);
  CHECK(attacked->other->kind == eng::game::LogicTargetKind::PLAYER);
}

TEST_CASE("game logic hears an actor enter a state, by its behavior or the "
          "logic's, by the state's id") {
  std::vector<std::string> states;
  Listening logic([&](GameLogicWorld& world) {
    if (world.tick() == 0) {
      REQUIRE(world.setActorState(world.actor(0).target, "search"));
    }
    for (const LogicEvent& event : world.events()) {
      if (event.kind == LogicEventKind::ACTOR_STATE_ENTERED) {
        states.emplace_back(event.state);
      }
    }
  });
  GameWorld world(chaserBeside(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  run(simulation, 2);

  // On tick 0 the chaser sees the player and pursues; then the logic sends
  // it searching.
  CHECK(states == std::vector<std::string>{"pursue", "search"});
}

namespace {

/// Two players side by side, near enough for either to revive the other.
GameSetup twoPlayers() {
  GameSetup setup;
  setup.player_count = 2;
  setup.spawns[0] = {1.5F, 1.5F, 0.0F};
  setup.spawns[1] = {2.0F, 1.5F, 0.0F};
  return setup;
}

}  // namespace

TEST_CASE("game logic hears a downed player revived, and by whom") {
  Listening logic([](GameLogicWorld& world) {
    if (world.tick() == 0) {
      world.damage(world.player(0).target, 999);
    }
  });
  GameWorld world(twoPlayers(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  run(simulation, eng::game::PLAYER_REVIVE_TICKS + 5);

  const LogicEvent* revived = logic.first(LogicEventKind::PLAYER_REVIVED);
  REQUIRE(revived != nullptr);
  CHECK(revived->target.index == world.players().slots.handleAt(0).index);
  CHECK(revived->by->index == world.players().slots.handleAt(1).index);
  CHECK(logic.first(LogicEventKind::PLAYER_OUT) == nullptr);
}

TEST_CASE("game logic is told once that the run is over, and how") {
  Listening logic([](GameLogicWorld& world) {
    if (world.tick() == 3) {
      world.endRun(RunOutcome::WON);
    }
  });
  GameWorld world(chaserBeside(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  run(simulation, 10);

  CHECK(logic.ends_ == 1);
  CHECK(logic.outcome_ == RunOutcome::WON);
  CHECK(world.takeLogicLog() == std::vector<std::string>{"the run is over"});
}
