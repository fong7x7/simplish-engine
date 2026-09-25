#include "sdk-rig.h"

#include <engine/input/input-action.h>
#include <engine/input/player-input-builder.h>
#include <engine/sim/simulation.h>
#include <game/world/game-world.h>
#include <utility>

namespace eng::game::sdk::test {

namespace {

  /// An idle actor named @p id of side @p faction at (@p x, @p y).
  ActorSpawn idle(const char* id, Faction faction, float x, float y) {
    ActorSpawn actor;
    actor.at = {x, y, 0.0F};
    actor.behavior = "idle";
    actor.faction = faction;
    actor.id = id;
    return actor;
  }

}  // namespace

ScriptedLogic::ScriptedLogic(std::function<void(GameLogicWorld&)> each)
  : each_(std::move(each)) {}

void ScriptedLogic::tick(GameLogicWorld& world) {
  each_(world);
}

GameSetup sdkArena() {
  GameSetup setup;
  setup.spawns[0] = {1.5F, 1.5F, 0.0F};
  setup.actors.push_back(idle("grunt_a", Faction::HOSTILE, 4.5F, 1.5F));
  setup.actors.push_back(idle("grunt_b", Faction::HOSTILE, 8.5F, 1.5F));
  setup.actors.push_back(idle("villager", Faction::NEUTRAL, 1.5F, 5.5F));
  setup.obstacles.push_back({{12.0F, -4.0F, 0.0F}, {13.0F, 8.0F, 2.0F}});
  setup.actor_capacity = 32;
  return setup;
}

uint32_t runLogicFiring(GameLogic& logic, int ticks) {
  GameWorld world(sdkArena(), {}, &logic);
  sim::Simulation simulation(world, sim::TickHashing::ON);
  sim::TickInput input;
  input.players[0].buttons = input::INPUT_BUTTON_FIRE;
  input.players[0].aim_x = input::INPUT_AXIS_MAX;
  for (int i = 0; i < ticks; ++i) {
    (void)simulation.step(input);
  }
  return world.projectilePool().slots.size();
}

void runLogic(GameLogic& logic, int ticks, const GameSetup& setup,
              const GameContent& content) {
  GameWorld world(setup, content, &logic);
  sim::Simulation simulation(world, sim::TickHashing::ON);
  for (int i = 0; i < ticks; ++i) {
    (void)simulation.step({});
  }
}

}  // namespace eng::game::sdk::test
