#include <algorithm>
#include <engine/spatial/nav-grid-fit.h>
#include <game/actors/actor-system.h>
#include <game/actors/actor-tick-context.h>
#include <game/content/behavior-lookup.h>
#include <game/content/character-lookup.h>
#include <game/player/player-system.h>
#include <game/world/game-world.h>

namespace eng::game {

namespace {

  /// How many players @p setup holds, held to 1 through `sim::MAX_PLAYERS`.
  uint8_t playerCount(const GameSetup& setup) {
    return static_cast<uint8_t>(
        std::clamp<size_t>(setup.player_count, 1, sim::MAX_PLAYERS));
  }

  /// The navigation grid for @p setup's actors: its obstacles, over a
  /// rectangle taking in every spawn. No cells when there are no actors
  /// to plan across it.
  spatial::NavGrid navGridFor(const GameSetup& setup) {
    if (setup.actors.empty()) {
      return {};
    }
    std::vector<Vec2> points;
    points.reserve(sim::MAX_PLAYERS + setup.actors.size());
    for (uint8_t slot = 0; slot < playerCount(setup); ++slot) {
      points.push_back({setup.spawns[slot].x, setup.spawns[slot].y});
    }
    for (const ActorSpawn& actor : setup.actors) {
      points.push_back({actor.at.x, actor.at.y});
    }
    return {spatial::fitNavGrid(setup.obstacles, points, setup.spawns[0].z),
            setup.obstacles};
  }

}  // namespace

GameWorld::GameWorld(const GameSetup& setup, const GameContent& content)
  : obstacles_(setup.obstacles), grid_(navGridFor(setup)),
    actors_(static_cast<uint32_t>(setup.actors.size())),
    workspace_(static_cast<uint32_t>(setup.actors.size()), grid_.cellCount()),
    ai_rng_(setup.seed, AI_RNG_STREAM) {
  for (uint8_t slot = 0; slot < playerCount(setup); ++slot) {
    (void)spawnPlayer(players_, slot, setup.spawns[slot],
                      resolveCharacter(content, setup.characters[slot]));
  }
  spawnActors(setup, content);
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
                              .brains = brains_,
                              .rng = ai_rng_};
  stepActors(actors_, view, workspace_);
}

void GameWorld::compaction([[maybe_unused]] const sim::TickContext& context) {
  compactPlayers(players_);
  compactActors(actors_);
}

void GameWorld::hashState(sim::TickHashBuilder& builder) const {
  hashPlayers(players_, builder.section("players"));
  hashActors(actors_, builder.section("actors"));
  builder.section("ai_rng").add(ai_rng_.state());
}

void GameWorld::spawnActors(const GameSetup& setup,
                            const GameContent& content) {
  brains_.reserve(setup.actors.size());
  actor_handles_.reserve(setup.actors.size());
  for (const ActorSpawn& spawn : setup.actors) {
    const uint16_t brain = brainIndex(resolveBehavior(content, spawn.behavior));
    const auto handle = spawnActor(actors_, spawn, brain, brains_[brain]);
    actor_handles_.push_back(handle.value_or(sim::EntityHandle{}));
  }
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
