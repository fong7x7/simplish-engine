#include "world-logic-view.h"

#include <algorithm>
#include <engine/core/fixed-step-clock.h>
#include <engine/spatial/line-of-sight.h>
#include <game/actors/actor-spawn.h>
#include <game/actors/enemy-spawn.h>
#include <game/combat/combatant-ref.h>
#include <game/content/enemy-lookup.h>
#include <game/player/player-system.h>
#include <game/world/game-world.h>
#include <game/world/logic-combatant.h>
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

std::optional<uint32_t> WorldLogicView::actorIndex(LogicTarget target) const {
  if (target.kind != LogicTargetKind::ACTOR) {
    return std::nullopt;
  }
  return scene_.actors.slots.denseIndex({target.index, target.generation});
}

std::optional<LogicActor> WorldLogicView::actorOf(LogicTarget target) const {
  const std::optional<uint32_t> index = actorIndex(target);
  return index ? std::optional{actor(*index)} : std::nullopt;
}

std::optional<LogicPlayer> WorldLogicView::playerOf(LogicTarget target) const {
  if (target.kind != LogicTargetKind::PLAYER) {
    return std::nullopt;
  }
  const auto index =
      scene_.players.slots.denseIndex({target.index, target.generation});
  return index ? std::optional{player(*index)} : std::nullopt;
}

std::span<const LogicEvent> WorldLogicView::events() const {
  return scene_.events;
}

uint8_t WorldLogicView::clearance() const {
  return scene_.grid.requiredClearance(ACTOR_DEFAULT_RADIUS_TILES);
}

bool WorldLogicView::lineOfSight(Vec3 from, Vec3 to) const {
  const Vec2 a{from.x, from.y};
  const Vec2 b{to.x, to.y};
  return scene_.grid.cellAt(a) && scene_.grid.cellAt(b) &&
         spatial::hasLineOfSight(scene_.grid, a, b, clearance());
}

bool WorldLogicView::walkable(Vec3 at) const {
  const auto cell = scene_.grid.cellAt({at.x, at.y});
  return cell && scene_.grid.isOpen(*cell, clearance());
}

uint32_t WorldLogicView::obstacleCount() const {
  return static_cast<uint32_t>(scene_.obstacles.size());
}

physics::CollisionBox WorldLogicView::obstacle(uint32_t index) const {
  return index < scene_.obstacles.size() ? scene_.obstacles[index]
                                         : physics::CollisionBox{};
}

void WorldLogicView::damage(LogicTarget target, uint16_t amount,
                            std::optional<LogicTarget> by) {
  scene_.commands.push_back({.kind = LogicCommandKind::DAMAGE,
                             .target = target,
                             .amount = amount,
                             .by = by});
}

void WorldLogicView::heal(LogicTarget target, uint16_t amount) {
  scene_.commands.push_back(
      {.kind = LogicCommandKind::HEAL, .target = target, .amount = amount});
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

void WorldLogicView::moveTo(LogicTarget target, Vec3 at) {
  scene_.commands.push_back(
      {.kind = LogicCommandKind::MOVE, .target = target, .at = at});
}

void WorldLogicView::removeActor(LogicTarget target) {
  scene_.commands.push_back(
      {.kind = LogicCommandKind::REMOVE, .target = target});
}

bool WorldLogicView::setActorState(LogicTarget target, std::string_view state) {
  const std::optional<uint32_t> index = actorIndex(target);
  if (!index) {
    return false;
  }
  const auto& states =
      scene_.brains[scene_.actors.brain[*index]].behavior.states;
  const auto found = std::ranges::find(states, state, &BehaviorState::id);
  if (found == states.end()) {
    return false;
  }
  scene_.commands.push_back(
      {.kind = LogicCommandKind::SET_STATE,
       .target = target,
       .state = static_cast<uint8_t>(found - states.begin())});
  return true;
}

void WorldLogicView::setActorFaction(LogicTarget target, Faction faction) {
  scene_.commands.push_back({.kind = LogicCommandKind::SET_FACTION,
                             .target = target,
                             .faction = faction});
}

void WorldLogicView::fireShot(const LogicShot& shot) {
  if (Vec2::lengthSquared(shot.direction) == 0.0F) {
    return;
  }
  const float step = shot.speed / static_cast<float>(TICK_RATE_HZ);
  scene_.combat.shots.push_back({{shot.from.x, shot.from.y},
                                 Vec2::normalize(shot.direction) * step,
                                 shot.damage,
                                 shot.side,
                                 combatantOf(shot.shooter)});
}

void WorldLogicView::blast(const LogicBlast& blast) {
  scene_.combat.blasts.push_back({{blast.at.x, blast.at.y},
                                  blast.radius,
                                  blast.damage,
                                  NO_COMBATANT,
                                  combatantOf(blast.by)});
}

void WorldLogicView::spawnHazard(const LogicHazard& hazard) {
  scene_.combat.hazards.push_back({{hazard.at.x, hazard.at.y},
                                   hazard.radius,
                                   hazard.damage,
                                   hazard.ticks,
                                   hazard.side,
                                   combatantOf(hazard.by)});
}

uint32_t WorldLogicView::random(uint32_t bound) {
  return bound == 0 ? 0 : scene_.rng.nextBelow(bound);
}

void WorldLogicView::log(std::string_view message) {
  if (scene_.output.log.size() < WORLD_LOGIC_LOG_LINES) {
    scene_.output.log.emplace_back(message);
  }
}

void WorldLogicView::cue(const LogicCue& cue) {
  if (scene_.output.cues.size() < WORLD_LOGIC_CUES) {
    scene_.output.cues.push_back(
        {scene_.context.tick, cue.at, std::string(cue.sound),
         std::string(cue.effect), cue.gain, cue.scale, cue.reach});
  }
}

}  // namespace eng::game
