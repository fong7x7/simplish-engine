#include "support/actor-arena.h"

#include <game/actors/actor-system.h>
#include <game/content/character-definition.h>
#include <game/player/player-system.h>

namespace eng::game::test {

namespace {

  /// The arena's grid: 40 × 40 tiles from (-20, -20).
  spatial::NavGridSpec arenaSpec() {
    return {.origin = {-20.0F, -20.0F}, .width = 160, .height = 160};
  }

}  // namespace

ActorArena::ActorArena(std::vector<physics::CollisionBox> boxes)
  : obstacles(std::move(boxes)), grid(arenaSpec(), obstacles),
    workspace(16, grid.cellCount()) {
  brains.reserve(16);
}

uint32_t ActorArena::addPlayer(Vec2 at) {
  const auto slot = static_cast<uint8_t>(players.slots.size());
  (void)spawnPlayer(players, slot, {at.x, at.y, 0.0F}, CharacterDefinition{});
  return slot;
}

uint32_t ActorArena::addActor(const BehaviorDefinition& behavior,
                              ActorSpawn spawn) {
  brains.push_back(compileBrain(behavior));
  spawn.behavior = behavior.id;
  const auto brain = static_cast<uint16_t>(brains.size() - 1);
  (void)spawnActor(actors, spawn, brain, brains.back());
  return actors.slots.size() - 1;
}

void ActorArena::movePlayer(uint32_t player, Vec2 at) {
  players.position[player] = {at.x, at.y, 0.0F};
}

void ActorArena::setFiring(uint32_t player, uint32_t buttons) {
  input.players[players.input_slot[player]].buttons = buttons;
}

void ActorArena::step(uint32_t ticks) {
  for (uint32_t t = 0; t < ticks; ++t) {
    const ActorTickContext context{.tick = tick,
                                   .input = input,
                                   .players = players,
                                   .grid = grid,
                                   .obstacles = obstacles,
                                   .brains = brains,
                                   .rng = rng};
    stepActors(actors, context, workspace);
    ++tick;
  }
}

Vec2 ActorArena::actorAt(uint32_t actor) const {
  return {actors.position[actor].x, actors.position[actor].y};
}

const std::string& ActorArena::stateOf(uint32_t actor) const {
  return brains[actors.brain[actor]].behavior.states[actors.state[actor]].id;
}

uint64_t ActorArena::hash() const {
  sim::StateHasher hasher;
  hashActors(actors, hasher);
  hasher.add(rng.state());
  return hasher.value();
}

}  // namespace eng::game::test
