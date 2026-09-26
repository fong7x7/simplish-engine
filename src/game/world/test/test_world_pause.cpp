#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <engine/input/input-action.h>
#include <engine/input/player-input-builder.h>
#include <engine/sim/simulation.h>
#include <functional>
#include <game/logic/game-logic.h>
#include <game/player/player-system.h>
#include <game/world/game-world.h>
#include <utility>
#include <vector>

using eng::game::GameContent;
using eng::game::GameLogic;
using eng::game::GameLogicWorld;
using eng::game::GameSetup;
using eng::game::GameWorld;
using eng::game::LogicEvent;
using eng::game::LogicEventKind;
using eng::sim::Simulation;
using eng::sim::TickHashing;
using eng::sim::TickInput;

namespace {

/// A logic whose tick is whatever a test says, keeping every event kind
/// it hears.
class Pausing final : public GameLogic {
public:
  explicit Pausing(std::function<void(GameLogicWorld&)> each)
    : each_(std::move(each)) {}
  void tick(GameLogicWorld& world) override {
    for (const LogicEvent& event : world.events()) {
      heard_.push_back(event.kind);
    }
    each_(world);
  }
  /// How many events of @p kind it heard.
  [[nodiscard]] size_t heard(LogicEventKind kind) const {
    return static_cast<size_t>(std::ranges::count(heard_, kind));
  }

private:
  /// The test's tick.
  std::function<void(GameLogicWorld&)> each_;
  /// Every event heard, by kind.
  std::vector<LogicEventKind> heard_;
};

/// One player at (1.5, 1.5) and a hostile chaser a few steps off.
GameSetup chaserNear() {
  GameSetup setup;
  setup.spawns[0] = {1.5F, 1.5F, 0.0F};
  eng::game::ActorSpawn chaser;
  chaser.at = {5.5F, 1.5F, 0.0F};
  chaser.behavior = "chase";
  setup.actors.push_back(chaser);
  setup.actor_capacity = 8;
  return setup;
}

/// Player 1 walking along +X.
TickInput walking() {
  TickInput input;
  input.players[0].move_x = eng::input::INPUT_AXIS_MAX;
  return input;
}

/// Step @p simulation @p ticks times on @p input.
void run(Simulation& simulation, uint64_t ticks, const TickInput& input) {
  for (uint64_t i = 0; i < ticks; ++i) {
    (void)simulation.step(input);
  }
}

}  // namespace

TEST_CASE("a paused world stands still, and its play clock stops") {
  Pausing logic([](GameLogicWorld& world) {
    if (world.tick() == 0) {
      world.pause();
    }
  });
  GameWorld world(chaserNear(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);
  run(simulation, 1, walking());
  const float player_x = world.players().position[0].x;
  const float chaser_x = world.actors().position[0].x;

  run(simulation, 60, walking());

  CHECK(world.paused());
  CHECK(world.playTick() == 1);
  CHECK(world.players().position[0].x == player_x);
  CHECK(world.actors().position[0].x == chaser_x);
}

namespace {

/// Pause on the pause button; play on at any choice made on a screen.
void pauseOnButton(GameLogicWorld& world) {
  for (const LogicEvent& event : world.events()) {
    if (event.kind == LogicEventKind::PAUSE_PRESSED) {
      world.pause();
    } else if (event.kind == LogicEventKind::UI_ACTION) {
      world.resume();
    }
  }
}

/// Player 1 holding the pause button.
TickInput holdingPause() {
  TickInput input;
  input.players[0].buttons = eng::input::INPUT_BUTTON_PAUSE;
  return input;
}

}  // namespace

TEST_CASE("paused, the logic still hears the pause button and choices") {
  GameContent content;
  content.ui_actions = {"resume"};
  Pausing logic(pauseOnButton);
  GameWorld world(GameSetup{}, content, &logic);
  Simulation simulation(world, TickHashing::ON);
  TickInput resume;
  resume.players[0].ui_action = 1;

  run(simulation, 3, holdingPause());  // held three ticks: one press
  CHECK(world.paused());
  run(simulation, 1, resume);
  run(simulation, 2, TickInput{});

  CHECK(logic.heard(LogicEventKind::PAUSE_PRESSED) == 1);
  CHECK_FALSE(world.paused());
}

namespace {

/// Two players side by side.
GameSetup twoPlayers() {
  GameSetup setup;
  setup.player_count = 2;
  setup.spawns[0] = {1.5F, 1.5F, 0.0F};
  setup.spawns[1] = {2.0F, 1.5F, 0.0F};
  return setup;
}

/// Down player 1 and pause on tick 0; play on once a revive's worth of
/// ticks has gone by paused.
void downThenPause(GameLogicWorld& world) {
  if (world.tick() == 0) {
    world.damage(world.player(0).target, 999);
    world.pause();
  } else if (world.tick() == eng::game::PLAYER_REVIVE_TICKS + 10) {
    world.resume();
  }
}

}  // namespace

TEST_CASE("a pause runs down nothing: a revive waits for play") {
  Pausing logic(downThenPause);
  GameWorld world(twoPlayers(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  run(simulation, eng::game::PLAYER_REVIVE_TICKS + 10, TickInput{});
  CHECK(logic.heard(LogicEventKind::PLAYER_REVIVED) == 0);
  run(simulation, eng::game::PLAYER_REVIVE_TICKS + 5, TickInput{});

  CHECK(logic.heard(LogicEventKind::PLAYER_REVIVED) == 1);
}
