#include "world-logic-view.h"

#include <algorithm>
#include <game/actors/enemy-spawn.h>
#include <game/content/enemy-lookup.h>
#include <game/player/player-system.h>
#include <game/world/game-world.h>
#include <string>

namespace eng::game {

namespace {

  /// @p handle as game logic names an entity of pool @p kind.
  LogicTarget targetOf(LogicTargetKind kind, sim::EntityHandle handle) {
    return {kind, handle.index, handle.generation};
  }

  /// Where the player at dense index @p i stands in the run.
  LogicPlayerStatus statusOf(const PlayerPool& players, uint32_t i) {
    if (players.out[i] != 0) {
      return LogicPlayerStatus::OUT;
    }
    return players.downed[i] != 0 ? LogicPlayerStatus::DOWN
                                  : LogicPlayerStatus::UP;
  }

  /// The id of the state the actor at dense index @p i is in.
  std::string_view stateOf(const WorldLogicScene& scene, uint32_t i) {
    const uint16_t brain = scene.actors.brain[i];
    if (brain >= scene.brains.size()) {
      return {};
    }
    const auto& states = scene.brains[brain].behavior.states;
    const uint8_t state = scene.actors.state[i];
    return state < states.size() ? std::string_view(states[state].id)
                                 : std::string_view{};
  }

}  // namespace

uint64_t WorldLogicView::tick() const {
  return scene_.context.tick;
}

const sim::TickInput& WorldLogicView::input() const {
  return scene_.context.input;
}

uint32_t WorldLogicView::playerCount() const {
  return scene_.players.slots.size();
}

LogicPlayer WorldLogicView::player(uint32_t index) const {
  const PlayerPool& players = scene_.players;
  if (index >= players.slots.size()) {
    return {};
  }
  return {targetOf(LogicTargetKind::PLAYER, players.slots.handleAt(index)),
          players.input_slot[index],
          players.position[index],
          players.aim[index],
          players.health[index],
          players.max_health[index],
          statusOf(players, index)};
}

uint32_t WorldLogicView::actorCount() const {
  return scene_.actors.slots.size();
}

LogicActor WorldLogicView::actor(uint32_t index) const {
  const ActorPool& actors = scene_.actors;
  if (index >= actors.slots.size()) {
    return {};
  }
  const sim::EntityHandle handle = actors.slots.handleAt(index);
  const std::string_view id = handle.index < scene_.actor_ids.size()
                                  ? scene_.actor_ids[handle.index]
                                  : std::string_view{};
  return {targetOf(LogicTargetKind::ACTOR, handle),
          id,
          actors.position[index],
          actors.facing[index],
          actors.faction[index],
          actors.health[index],
          actors.max_health[index],
          stateOf(scene_, index)};
}

RunOutcome WorldLogicView::outcome() const {
  return scene_.outcome;
}

void WorldLogicView::damage(LogicTarget target, uint16_t amount) {
  scene_.commands.push_back({LogicCommandKind::DAMAGE, target, amount});
}

void WorldLogicView::heal(LogicTarget target, uint16_t amount) {
  scene_.commands.push_back({LogicCommandKind::HEAL, target, amount});
}

void WorldLogicView::endRun(RunOutcome outcome) {
  if (scene_.outcome == RunOutcome::PLAYING) {
    scene_.outcome = outcome;
  }
}

bool WorldLogicView::spawnEnemy(std::string_view archetype, Vec3 at,
                                std::string_view id) {
  const EnemyDefinition* enemy = findEnemy(scene_.content, archetype);
  if (enemy == nullptr) {
    return false;
  }
  ActorSpawn spawn = makeEnemySpawn(*enemy, at, 0.0F);
  spawn.id = std::string(id);
  return queueSpawn(std::move(spawn));
}

bool WorldLogicView::spawnActor(const LogicSpawn& spawn) {
  return queueSpawn({.at = spawn.at,
                     .yaw_degrees = spawn.yaw_degrees,
                     .behavior = std::string(spawn.behavior),
                     .faction = spawn.faction,
                     .health = std::max<uint16_t>(spawn.health, 1),
                     .id = std::string(spawn.id),
                     .model = std::string(spawn.model)});
}

uint32_t WorldLogicView::actorRoom() const {
  const uint32_t used =
      scene_.actors.slots.size() + static_cast<uint32_t>(scene_.spawns.size());
  const uint32_t capacity = scene_.actors.slots.capacity();
  return capacity > used ? capacity - used : 0;
}

bool WorldLogicView::queueSpawn(ActorSpawn spawn) {
  if (actorRoom() == 0) {
    return false;
  }
  scene_.spawns.push_back(std::move(spawn));
  return true;
}

uint32_t WorldLogicView::random(uint32_t bound) {
  return bound == 0 ? 0 : scene_.rng.nextBelow(bound);
}

void WorldLogicView::log(std::string_view message) {
  if (scene_.log.size() < WORLD_LOGIC_LOG_LINES) {
    scene_.log.emplace_back(message);
  }
}

}  // namespace eng::game
