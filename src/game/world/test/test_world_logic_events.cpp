#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <engine/input/player-input-builder.h>
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
using eng::game::LogicSteps;
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

namespace {

/// The combined hash of tick 2 in a world whose logic raises @p per_tick
/// cues a tick, keeping into @p kept the cues of ticks 0 and 1.
uint64_t hashWithCues(uint32_t per_tick,
                      std::vector<eng::game::WorldCue>& kept) {
  Listening logic([per_tick](GameLogicWorld& world) {
    for (uint32_t i = 0; i < per_tick; ++i) {
      world.cue({.at = {1.0F, 2.0F, 0.0F}, .sound = "combat.blast"});
    }
  });
  GameWorld world(chaserBeside(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);
  run(simulation, 2);
  kept = world.takeLogicCues();
  return simulation.step(TickInput{}).hash.value().combined;
}

}  // namespace

TEST_CASE("a cue the logic raises is kept for presentation, and changes "
          "nothing a tick hashes") {
  std::vector<eng::game::WorldCue> quiet;
  std::vector<eng::game::WorldCue> loud;

  CHECK(hashWithCues(0, quiet) == hashWithCues(1, loud));
  CHECK(quiet.empty());
  REQUIRE(loud.size() == 2);
  CHECK((loud[1].tick == 1 && loud[1].sound == "combat.blast"));
}

namespace {

/// Count the events of @p kind a logic listening for @p steps hears while
/// player 1 walks along +X for @p ticks.
size_t stepsHeard(LogicSteps steps, LogicEventKind kind, uint64_t ticks) {
  Listening logic([steps](GameLogicWorld& world) {
    if (world.tick() == 0) {
      world.listenForSteps(steps);
    }
  });
  GameWorld world(chaserBeside(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);
  TickInput walking;
  walking.players[0].move_x = eng::input::INPUT_AXIS_MAX;
  for (uint64_t i = 0; i < ticks; ++i) {
    (void)simulation.step(walking);
  }
  return static_cast<size_t>(
      std::ranges::count(logic.heard_, kind, &LogicEvent::kind));
}

}  // namespace

TEST_CASE("game logic hears the steps of whoever it listens to, and nobody's "
          "until it asks") {
  CHECK(stepsHeard(LogicSteps::NONE, LogicEventKind::PLAYER_STEPPED, 90) == 0);
  CHECK(stepsHeard(LogicSteps::PLAYERS, LogicEventKind::PLAYER_STEPPED, 90) >=
        2);
  CHECK(stepsHeard(LogicSteps::PLAYERS, LogicEventKind::ACTOR_STEPPED, 90) ==
        0);
  CHECK(stepsHeard(LogicSteps::EVERYONE, LogicEventKind::ACTOR_STEPPED, 90) >=
        1);
}

TEST_CASE("game logic told the run is over hears it lost when nobody is up") {
  Listening logic([](GameLogicWorld& world) {
    if (world.tick() == 0) {
      world.damage(world.player(0).target, 999);
    }
  });
  // One player alone: nothing bites first, to leave them in their grace.
  GameWorld world(GameSetup{}, {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  run(simulation, 5);

  CHECK(logic.ends_ == 1);
  CHECK(logic.outcome_ == RunOutcome::LOST);
}
