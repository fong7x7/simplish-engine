#include "world-logic-hash.h"
#include "world-logic-view.h"
#include "world-read-view.h"

#include <algorithm>
#include <game/actors/actor-system.h>
#include <game/actors/actor-tick-context.h>
#include <game/combat/combat-system.h>
#include <game/content/behavior-lookup.h>
#include <game/content/character-lookup.h>
#include <game/content/step-set-stride.h>
#include <game/player/player-system.h>
#include <game/world/game-world.h>
#include <game/world/logic-combatant.h>
#include <game/world/world-nav-grid.h>
#include <utility>

namespace eng::game {

namespace {

  /// What game logic hears of @p hit, which took @p lost from whoever it
  /// struck, standing at @p at: a @p kind.
  LogicEvent hitEvent(const DamageEvent& hit, LogicEventKind kind, Vec3 at,
                      uint16_t lost) {
    return {.kind = kind,
            .target = *logicTargetOf(hit.target),
            .at = at,
            .by = logicTargetOf(hit.source),
            .amount = lost,
            .cause = logicCauseOf(hit.cause)};
  }

  /// How many players @p setup holds, held to 1 through `sim::MAX_PLAYERS`.
  uint8_t playerCount(const GameSetup& setup) {
    return static_cast<uint8_t>(
        std::clamp<size_t>(setup.player_count, 1, sim::MAX_PLAYERS));
  }

  /// Every player who is up, as a body a shot or a blast can catch, into
  /// @p bodies.
  void listPlayerBodies(const PlayerPool& players,
                        std::vector<CombatBody>& bodies) {
    for (uint32_t p = 0; p < players.slots.size(); ++p) {
      if (playerIsUp(players, p)) {
        const Vec3& at = players.position[p];
        bodies.push_back({{CombatantKind::PLAYER, players.slots.handleAt(p)},
                          {at.x, at.y},
                          PLAYER_RADIUS_TILES,
                          Faction::FRIENDLY});
      }
    }
  }

  /// Every actor still alive, as a body, into @p bodies.
  void listActorBodies(const ActorPool& actors,
                       std::vector<CombatBody>& bodies) {
    for (uint32_t i = 0; i < actors.slots.size(); ++i) {
      if (actors.health[i] != 0) {
        const Vec3& at = actors.position[i];
        bodies.push_back({{CombatantKind::ACTOR, actors.slots.handleAt(i)},
                          {at.x, at.y},
                          actors.radius[i],
                          actors.faction[i]});
      }
    }
  }

  /// Room in @p effects for a tick of @p actors actors attacking — a hit or
  /// a shot volley each, and a blast or pool for some — so appending does
  /// not allocate.
  void reserveEffects(CombatEffects& effects, size_t actors) {
    effects.damage.reserve(actors * 2 + 64);
    effects.blasts.reserve(actors + 8);
    effects.shots.reserve(actors * 4 + 16);
    effects.hazards.reserve(actors + 8);
  }

  /// Cues a tick is given room for: every projectile in flight landing,
  /// every volley the effects have room for fired, and a blast apiece. A
  /// cue past this is dropped, never grown into.
  size_t cueRoom(size_t actors) {
    return PROJECTILE_POOL_CAPACITY + actors * 5 + 64;
  }

  /// The pool a game logic target names an entity of.
  CombatantKind combatantKindOf(const LogicTarget& target) {
    return target.kind == LogicTargetKind::PLAYER ? CombatantKind::PLAYER
                                                  : CombatantKind::ACTOR;
  }

  /// The most actors @p setup's run holds: its own, or the room it asks
  /// for when that is more.
  uint32_t actorCapacity(const GameSetup& setup) {
    return std::max(static_cast<uint32_t>(setup.actors.size()),
                    setup.actor_capacity);
  }

  /// The navigation grid for @p setup's actors, or one with no cells when
  /// there can be none to plan across it.
  spatial::NavGrid navGridFor(const GameSetup& setup) {
    return actorCapacity(setup) == 0 ? spatial::NavGrid{}
                                     : buildWorldNavGrid(setup);
  }

}  // namespace

GameWorld::GameWorld(const GameSetup& setup, const GameContent& content,
                     GameLogic* logic)
  : obstacles_(setup.obstacles), grid_(navGridFor(setup)),
    broadphase_(obstacles_), actors_(actorCapacity(setup)),
    flow_(grid_, grid_.requiredClearance(ACTOR_DEFAULT_RADIUS_TILES)),
    combat_(actorCapacity(setup) + sim::MAX_PLAYERS, grid_.spec(), broadphase_),
    workspace_(actorCapacity(setup), grid_, broadphase_),
    ai_rng_(setup.seed, AI_RNG_STREAM), logic_(logic),
    logic_rng_(setup.seed, LOGIC_RNG_STREAM),
    content_(actorCapacity(setup) > setup.actors.size() ? content
                                                        : GameContent{}) {
  for (uint8_t slot = 0; slot < playerCount(setup); ++slot) {
    const CharacterDefinition& character =
        resolveCharacter(content, setup.characters[slot]);
    if (const auto handle =
            spawnPlayer(players_, slot, setup.spawns[slot], character)) {
      player_gaits_[handle->index].stride = stepSetStride(character.footsteps);
    }
  }
  spawnActors(setup, content);
  reserveEffects(effects_, actorCapacity(setup));
  actor_notes_.reserve(actorCapacity(setup));
  cues_.reserve(cueRoom(actorCapacity(setup)));
}

void GameWorld::playerControl(const sim::TickContext& context) {
  cues_.clear();
  movePlayers(players_, context.input, obstacles_);
}

void GameWorld::enemyAi(const sim::TickContext& context) {
  const ActorTickContext view{.tick = context.tick,
                              .input = context.input,
                              .players = players_,
                              .grid = grid_,
                              .obstacles = obstacles_,
                              .broadphase = broadphase_,
                              .brains = brains_,
                              .routes = routes_,
                              .flow = flow_,
                              .effects = effects_,
                              .notes = actor_notes_,
                              .rng = ai_rng_};
  stepActors(actors_, view, workspace_);
  noteActorEvents();
  noteSteps();
}

void GameWorld::noteSteps() {
  if (logic_ == nullptr || logic_steps_ == LogicSteps::NONE) {
    return;
  }
  notePlayerSteps();
  if (logic_steps_ == LogicSteps::EVERYONE) {
    noteActorSteps();
  }
}

void GameWorld::notePlayerSteps() {
  for (uint32_t p = 0; p < players_.slots.size(); ++p) {
    const sim::EntityHandle handle = players_.slots.handleAt(p);
    if (walkOn(player_gaits_[handle.index], players_.position[p])) {
      logic_events_.note(
          {.kind = LogicEventKind::PLAYER_STEPPED,
           .target = *logicTargetOf({CombatantKind::PLAYER, handle}),
           .at = players_.position[p]});
    }
  }
}

void GameWorld::noteActorSteps() {
  for (uint32_t i = 0; i < actors_.slots.size(); ++i) {
    const sim::EntityHandle handle = actors_.slots.handleAt(i);
    if (actors_.health[i] != 0 &&
        walkOn(actor_gaits_[handle.index], actors_.position[i])) {
      logic_events_.note(
          {.kind = LogicEventKind::ACTOR_STEPPED,
           .target = *logicTargetOf({CombatantKind::ACTOR, handle}),
           .at = actors_.position[i],
           .id = actor_ids_[handle.index]});
    }
  }
}

void GameWorld::hashGaits(sim::StateHasher& hasher) const {
  hasher.add(logic_steps_);
  const auto hashOne = [&hasher](const WalkerGait& gait) {
    hasher.add(gait.last);
    hasher.add(gait.travelled);
    hasher.add(gait.known);
  };
  for (uint32_t p = 0; p < players_.slots.size(); ++p) {
    hashOne(player_gaits_[players_.slots.handleAt(p).index]);
  }
  for (uint32_t i = 0; i < actors_.slots.size(); ++i) {
    hashOne(actor_gaits_[actors_.slots.handleAt(i).index]);
  }
}

void GameWorld::noteActorEvents() {
  for (const ActorNote& note : actor_notes_) {
    const auto index = actors_.slots.denseIndex(note.actor);
    if (logic_ != nullptr && index) {
      logic_events_.note(actorEvent(note, *index));
    }
  }
  actor_notes_.clear();
}

LogicEvent GameWorld::actorEvent(const ActorNote& note, uint32_t index) const {
  // In `ActorNoteKind` order.
  static constexpr LogicEventKind KINDS[] = {
      LogicEventKind::ACTOR_STATE_ENTERED, LogicEventKind::ACTOR_NOTICED,
      LogicEventKind::ACTOR_ATTACKED, LogicEventKind::ACTOR_WINDING_UP};
  const bool entered = note.kind == ActorNoteKind::STATE_ENTERED;
  return {.kind = KINDS[static_cast<size_t>(note.kind)],
          .target = {LogicTargetKind::ACTOR, note.actor.index,
                     note.actor.generation},
          .at = actors_.position[index],
          .id = actor_ids_[note.actor.index],
          .other = logicTargetOf(note.other),
          .state = entered ? std::string_view(brains_[actors_.brain[index]]
                                                  .behavior.states[note.state]
                                                  .id)
                           : std::string_view{}};
}

void GameWorld::notePlayerChanges(std::span<const PlayerChange> changes) {
  for (const PlayerChange& change : changes) {
    const auto index = players_.slots.denseIndex(change.player);
    if (logic_ == nullptr || !index) {
      continue;
    }
    logic_events_.note(
        {.kind = change.kind == PlayerChangeKind::REVIVED
                     ? LogicEventKind::PLAYER_REVIVED
                     : LogicEventKind::PLAYER_OUT,
         .target = *logicTargetOf({CombatantKind::PLAYER, change.player}),
         .at = players_.position[*index],
         .by = change.by ? logicTargetOf({CombatantKind::PLAYER, *change.by})
                         : std::nullopt});
  }
}

void GameWorld::weaponFire([[maybe_unused]] const sim::TickContext& context) {
  spawnCombatEffects(projectiles_, hazards_, effects_, cues_);
  effects_.shots.clear();
  effects_.hazards.clear();
}

void GameWorld::projectiles(const sim::TickContext& context) {
  const CombatScene scene = combatScene(context.tick);
  stepProjectiles(projectiles_, scene);
  stepHazards(hazards_, scene);
}

void GameWorld::damage(const sim::TickContext& context) {
  resolveDamage(context.tick);
  notePlayerChanges(updateDownedPlayers(players_, context.tick));
  clearCombatEffects(effects_);
}

void GameWorld::resolveDamage(uint64_t tick) {
  const CombatScene scene = combatScene(tick);
  // A death can set off a blast, and a blast can kill: go round until
  // neither leaves anything to do. Each actor dies once, so it ends.
  resolveBlasts(scene);
  size_t next = 0;
  while (next < effects_.damage.size()) {
    for (; next < effects_.damage.size(); ++next) {
      applyHit(effects_.damage[next], tick);
    }
    resolveBlasts(scene);
  }
}

void GameWorld::director(const sim::TickContext& context) {
  if (logic_ == nullptr) {
    return;
  }
  runLogic(context, LogicCall::TICK);
  applyLogicWrites(context.tick);
}

void GameWorld::applyLogicWrites(uint64_t tick) {
  for (const LogicCommand& command : logic_commands_) {
    applyLogicCommand(command, tick);
  }
  logic_commands_.clear();
  // After any the logic's damage set off: blasts go off in order.
  effects_.blasts.insert(effects_.blasts.end(), logic_effects_.blasts.begin(),
                         logic_effects_.blasts.end());
  resolveDamage(tick);
  clearCombatEffects(effects_);
  // What the logic fired flies, and what it spilled bites, from next tick.
  game::spawnCombatEffects(projectiles_, hazards_, logic_effects_, cues_);
  clearCombatEffects(logic_effects_);
  for (const ActorSpawn& spawn : logic_spawns_) {
    addActor(spawn);
  }
  logic_spawns_.clear();
}

void GameWorld::addActor(const ActorSpawn& spawn) {
  const uint16_t brain = brainIndex(resolveBehavior(content_, spawn.behavior));
  const auto handle = spawnActor(actors_, spawn, brain, brains_[brain]);
  if (handle) {
    assignRoute(actors_.slots.size() - 1U, spawn.route);
    actor_ids_[handle->index] = spawn.id;
    actor_models_[handle->index] = spawn.model;
    actor_spawned_[handle->index] = 1;
    actor_gaits_[handle->index] = {.stride = stepSetStride(spawn.footsteps)};
    logic_events_.note(
        {LogicEventKind::ACTOR_SPAWNED,
         {LogicTargetKind::ACTOR, handle->index, handle->generation},
         spawn.at,
         spawn.id});
  }
}

std::unique_ptr<GameLogicWorld> GameWorld::readView(uint64_t tick) const {
  return std::make_unique<WorldReadView>(WorldReadSources{
      players_, actors_, brains_, actor_ids_, content_, grid_, obstacles_,
      logic_events_.events(), tick, logic_rng_, outcome()});
}

std::string_view GameWorld::actorId(uint32_t index) const {
  return actor_ids_[actors_.slots.handleAt(index).index];
}

std::string_view GameWorld::actorModel(uint32_t index) const {
  return actor_models_[actors_.slots.handleAt(index).index];
}

bool GameWorld::actorSpawned(uint32_t index) const {
  return actor_spawned_[actors_.slots.handleAt(index).index] != 0;
}

void GameWorld::runLogic(const sim::TickContext& context, LogicCall call) {
  WorldLogicView view({.context = context,
                       .players = players_,
                       .actors = actors_,
                       .brains = brains_,
                       .actor_ids = actor_ids_,
                       .commands = logic_commands_,
                       .spawns = logic_spawns_,
                       .combat = logic_effects_,
                       .content = content_,
                       .grid = grid_,
                       .obstacles = obstacles_,
                       .events = logic_events_.events(),
                       .rng = logic_rng_,
                       .run = {logic_outcome_, logic_steps_},
                       .output = {logic_log_, logic_cues_}});
  callLogic(view, context.tick, call);
}

void GameWorld::callLogic(GameLogicWorld& view, uint64_t tick, LogicCall call) {
  if (call == LogicCall::END) {
    logic_->end(view);
    return;
  }
  if (tick == 0) {
    logic_->start(view);
  }
  logic_->tick(view);
}

void GameWorld::applyLogicCommand(const LogicCommand& command, uint64_t tick) {
  const LogicTarget& target = command.target;
  const sim::EntityHandle handle{target.index, target.generation};
  if (command.kind == LogicCommandKind::HEAL) {
    heal(target, command.amount);
  } else if (command.kind == LogicCommandKind::DAMAGE) {
    applyHit({{combatantKindOf(target), handle},
              command.amount,
              combatantOf(command.by),
              DamageCause::LOGIC},
             tick);
  } else if (command.kind == LogicCommandKind::MOVE) {
    moveTo(target, command.at);
  } else if (const auto i = actors_.slots.denseIndex(handle);
             i && target.kind == LogicTargetKind::ACTOR) {
    applyActorCommand(command, *i, tick);
  }
}

void GameWorld::applyActorCommand(const LogicCommand& command, uint32_t index,
                                  uint64_t tick) {
  if (command.kind == LogicCommandKind::REMOVE && actors_.health[index] != 0) {
    logic_events_.note({LogicEventKind::ACTOR_REMOVED, command.target,
                        actors_.position[index],
                        actor_ids_[command.target.index]});
    removeActor(actors_, index);
  } else if (command.kind == LogicCommandKind::SET_STATE) {
    enterLogicState(index, command.state, tick);
  } else if (command.kind == LogicCommandKind::SET_FACTION) {
    actors_.faction[index] = command.faction;
  }
}

void GameWorld::enterLogicState(uint32_t index, uint8_t state, uint64_t tick) {
  actors_.state[index] = state;
  actors_.state_since[index] = tick;
  actors_.has_goal[index] = 0;
  actors_.attack_lands_tick[index] = ACTOR_NOT_WINDING;
  logic_events_.note(actorEvent({.kind = ActorNoteKind::STATE_ENTERED,
                                 .actor = actors_.slots.handleAt(index),
                                 .state = state},
                                index));
}

void GameWorld::moveTo(const LogicTarget& target, Vec3 at) {
  const sim::EntityHandle handle{target.index, target.generation};
  if (target.kind == LogicTargetKind::PLAYER) {
    if (const auto p = players_.slots.denseIndex(handle)) {
      players_.position[*p] = at;
    }
  } else if (const auto i = actors_.slots.denseIndex(handle)) {
    actors_.position[*i] = at;
    actors_.path[*i] = {};
    actors_.has_goal[*i] = 0;
  }
}

void GameWorld::heal(const LogicTarget& target, uint16_t amount) {
  const sim::EntityHandle handle{target.index, target.generation};
  if (target.kind == LogicTargetKind::PLAYER) {
    if (const auto p = players_.slots.denseIndex(handle)) {
      healPlayer(players_, *p, amount);
    }
  } else if (const auto i = actors_.slots.denseIndex(handle)) {
    healActor(actors_, *i, amount);
  }
}

bool GameWorld::runOver() const {
  return outcome() != RunOutcome::PLAYING;
}

RunOutcome GameWorld::outcome() const {
  if (logic_outcome_ != RunOutcome::PLAYING) {
    return logic_outcome_;
  }
  for (uint32_t p = 0; p < players_.slots.size(); ++p) {
    if (playerIsUp(players_, p)) {
      return RunOutcome::PLAYING;
    }
  }
  return RunOutcome::LOST;
}

std::vector<WorldCue> GameWorld::takeLogicCues() {
  return std::exchange(logic_cues_, {});
}

std::vector<std::string> GameWorld::takeLogicLog() {
  std::vector<std::string> lines;
  lines.swap(logic_log_);
  return lines;
}

void GameWorld::endLogic(const sim::TickContext& context) {
  if (logic_ended_ != 0 || !runOver()) {
    return;
  }
  logic_ended_ = 1;
  runLogic(context, LogicCall::END);
  logic_commands_.clear();
  logic_spawns_.clear();
  clearCombatEffects(logic_effects_);
}

void GameWorld::compaction(const sim::TickContext& context) {
  if (logic_ != nullptr) {
    logic_events_.publish();
    endLogic(context);
  }
  compactPlayers(players_);
  compactActors(actors_);
  compactProjectiles(projectiles_);
  compactHazards(hazards_);
}

void GameWorld::hashState(sim::TickHashBuilder& builder) const {
  hashPlayers(players_, builder.section("players"));
  hashActors(actors_, builder.section("actors"));
  hashFlowFields(flow_, builder.section("flow"));
  hashProjectiles(projectiles_, builder.section("projectiles"));
  hashHazards(hazards_, builder.section("hazards"));
  builder.section("ai_rng").add(ai_rng_.state());
  if (logic_ != nullptr) {
    sim::StateHasher& section = builder.section("logic");
    section.add(logic_outcome_);
    section.add(logic_ended_);
    hashGaits(section);
    section.add(logic_rng_.state());
    logic_events_.hashInto(section);
    WorldLogicHash hash(section);
    logic_->hashState(hash);
  }
}

CombatScene GameWorld::combatScene(uint64_t tick) {
  listBodies();
  indexCombatBodies(combat_);
  return {tick, obstacles_, broadphase_, combat_, effects_, cues_};
}

void GameWorld::listBodies() {
  combat_.bodies.clear();
  listPlayerBodies(players_, combat_.bodies);
  listActorBodies(actors_, combat_.bodies);
}

void GameWorld::applyHit(const DamageEvent& hit, uint64_t tick) {
  if (hit.target.kind == CombatantKind::PLAYER) {
    if (const auto p = players_.slots.denseIndex(hit.target.handle)) {
      hitPlayer(hit, *p, tick);
    }
  } else if (const auto i = actors_.slots.denseIndex(hit.target.handle)) {
    hitActor(hit, *i, tick);
  }
}

void GameWorld::hitPlayer(const DamageEvent& hit, uint32_t index,
                          uint64_t tick) {
  const uint16_t before = players_.health[index];
  hurtPlayer(players_, index, hit.amount, tick);
  const auto lost = static_cast<uint16_t>(before - players_.health[index]);
  if (logic_ != nullptr && lost != 0) {
    logic_events_.note(hitEvent(hit,
                                playerIsUp(players_, index)
                                    ? LogicEventKind::PLAYER_HURT
                                    : LogicEventKind::PLAYER_DOWNED,
                                players_.position[index], lost));
  }
}

void GameWorld::hitActor(const DamageEvent& hit, uint32_t index,
                         uint64_t tick) {
  const uint16_t before = actors_.health[index];
  hurtActor(actors_, index, {hit.amount, tick, hit.source}, effects_);
  const auto lost = static_cast<uint16_t>(before - actors_.health[index]);
  if (logic_ != nullptr && lost != 0) {
    LogicEvent event =
        hitEvent(hit,
                 actors_.health[index] == 0 ? LogicEventKind::ACTOR_DIED
                                            : LogicEventKind::ACTOR_HURT,
                 actors_.position[index], lost);
    event.id = actor_ids_[hit.target.handle.index];
    logic_events_.note(event);
  }
}

void GameWorld::sizeActorSlots(uint32_t capacity) {
  actor_ids_.resize(capacity);
  actor_models_.resize(capacity);
  actor_spawned_.resize(capacity);
  actor_gaits_.resize(capacity);
}

void GameWorld::spawnActors(const GameSetup& setup,
                            const GameContent& content) {
  brains_.reserve(setup.actors.size());
  routes_.reserve(setup.actors.size());
  actor_handles_.reserve(setup.actors.size());
  sizeActorSlots(actorCapacity(setup));
  for (const ActorSpawn& spawn : setup.actors) {
    const uint16_t brain = brainIndex(resolveBehavior(content, spawn.behavior));
    const auto handle = spawnActor(actors_, spawn, brain, brains_[brain]);
    actor_handles_.push_back(handle.value_or(sim::EntityHandle{}));
    if (handle) {
      assignRoute(actors_.slots.size() - 1U, spawn.route);
      actor_ids_[handle->index] = spawn.id;
      actor_models_[handle->index] = spawn.model;
      actor_gaits_[handle->index] = {.stride = stepSetStride(spawn.footsteps)};
    }
  }
}

void GameWorld::assignRoute(uint32_t index, const std::vector<Vec2>& points) {
  if (points.empty()) {
    return;
  }
  actors_.route[index] = static_cast<uint16_t>(routes_.size());
  routes_.push_back({points});
}

uint16_t GameWorld::brainIndex(const BehaviorDefinition& behavior) {
  for (size_t i = 0; i < brains_.size(); ++i) {
    if (brains_[i].behavior.id == behavior.id) {
      return static_cast<uint16_t>(i);
    }
  }
  brains_.push_back(compileBrain(behavior));
  return static_cast<uint16_t>(brains_.size() - 1);
}

}  // namespace eng::game
