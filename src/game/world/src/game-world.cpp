#include <algorithm>
#include <game/actors/actor-system.h>
#include <game/actors/actor-tick-context.h>
#include <game/combat/combat-system.h>
#include <game/content/behavior-lookup.h>
#include <game/content/character-lookup.h>
#include <game/player/player-system.h>
#include <game/world/game-world.h>
#include <game/world/world-nav-grid.h>

namespace eng::game {

namespace {

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

  /// The navigation grid for @p setup's actors, or one with no cells when
  /// there are none to plan across it.
  spatial::NavGrid navGridFor(const GameSetup& setup) {
    return setup.actors.empty() ? spatial::NavGrid{} : buildWorldNavGrid(setup);
  }

}  // namespace

GameWorld::GameWorld(const GameSetup& setup, const GameContent& content)
  : obstacles_(setup.obstacles), grid_(navGridFor(setup)),
    broadphase_(obstacles_),
    actors_(static_cast<uint32_t>(setup.actors.size())),
    flow_(grid_, grid_.requiredClearance(ACTOR_DEFAULT_RADIUS_TILES)),
    combat_(static_cast<uint32_t>(setup.actors.size() + sim::MAX_PLAYERS),
            grid_.spec(), broadphase_),
    workspace_(static_cast<uint32_t>(setup.actors.size()), grid_, broadphase_),
    ai_rng_(setup.seed, AI_RNG_STREAM) {
  for (uint8_t slot = 0; slot < playerCount(setup); ++slot) {
    (void)spawnPlayer(players_, slot, setup.spawns[slot],
                      resolveCharacter(content, setup.characters[slot]));
  }
  spawnActors(setup, content);
  reserveEffects(effects_, setup.actors.size());
}

void GameWorld::playerControl(const sim::TickContext& context) {
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
                              .rng = ai_rng_};
  stepActors(actors_, view, workspace_);
}

void GameWorld::weaponFire([[maybe_unused]] const sim::TickContext& context) {
  spawnCombatEffects(projectiles_, hazards_, effects_);
  effects_.shots.clear();
  effects_.hazards.clear();
}

void GameWorld::projectiles(const sim::TickContext& context) {
  const CombatScene scene = combatScene(context.tick);
  stepProjectiles(projectiles_, scene);
  stepHazards(hazards_, scene);
}

void GameWorld::damage(const sim::TickContext& context) {
  const CombatScene scene = combatScene(context.tick);
  // A death can set off a blast, and a blast can kill: go round until
  // neither leaves anything to do. Each actor dies once, so it ends.
  resolveBlasts(scene);
  size_t next = 0;
  while (next < effects_.damage.size()) {
    for (; next < effects_.damage.size(); ++next) {
      applyHit(effects_.damage[next], context.tick);
    }
    resolveBlasts(scene);
  }
  updateDownedPlayers(players_, context.tick);
  clearCombatEffects(effects_);
}

bool GameWorld::runOver() const {
  for (uint32_t p = 0; p < players_.slots.size(); ++p) {
    if (playerIsUp(players_, p)) {
      return false;
    }
  }
  return true;
}

void GameWorld::compaction([[maybe_unused]] const sim::TickContext& context) {
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
}

CombatScene GameWorld::combatScene(uint64_t tick) {
  listBodies();
  indexCombatBodies(combat_);
  return {tick, obstacles_, broadphase_, combat_, effects_};
}

void GameWorld::listBodies() {
  combat_.bodies.clear();
  listPlayerBodies(players_, combat_.bodies);
  listActorBodies(actors_, combat_.bodies);
}

void GameWorld::applyHit(const DamageEvent& hit, uint64_t tick) {
  if (hit.target.kind == CombatantKind::PLAYER) {
    if (const auto p = players_.slots.denseIndex(hit.target.handle)) {
      hurtPlayer(players_, *p, hit.amount, tick);
    }
  } else if (const auto i = actors_.slots.denseIndex(hit.target.handle)) {
    hurtActor(actors_, *i, {hit.amount, tick}, effects_);
  }
}

void GameWorld::spawnActors(const GameSetup& setup,
                            const GameContent& content) {
  brains_.reserve(setup.actors.size());
  routes_.reserve(setup.actors.size());
  actor_handles_.reserve(setup.actors.size());
  for (const ActorSpawn& spawn : setup.actors) {
    const uint16_t brain = brainIndex(resolveBehavior(content, spawn.behavior));
    const auto handle = spawnActor(actors_, spawn, brain, brains_[brain]);
    actor_handles_.push_back(handle.value_or(sim::EntityHandle{}));
    if (handle) {
      assignRoute(actors_.slots.size() - 1U, spawn.route);
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
