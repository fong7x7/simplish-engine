#include "horde-scenario.h"

#include <engine/core/pcg32.h>
#include <engine/input/input-action.h>
#include <engine/input/player-input-builder.h>
#include <engine/math/sin-cos.h>
#include <engine/physics/cylinder-collision.h>
#include <game/actors/enemy-spawn.h>
#include <game/content/behavior-lookup.h>

namespace eng::game::test {

namespace {

  /// Half the arena's side, in tiles.
  constexpr float ARENA_HALF = 32.0F;

  /// Tiles between pillars.
  constexpr int PILLAR_SPACING = 6;

  /// The four walls round the arena, a tile thick.
  void addWalls(GameSetup& setup) {
    constexpr float H = ARENA_HALF;
    setup.obstacles.push_back({{-H - 1, -H - 1, 0}, {H + 1, -H, 2}});
    setup.obstacles.push_back({{-H - 1, H, 0}, {H + 1, H + 1, 2}});
    setup.obstacles.push_back({{-H - 1, -H, 0}, {-H, H, 2}});
    setup.obstacles.push_back({{H, -H, 0}, {H + 1, H, 2}});
  }

  /// A pillar a tile across at every `PILLAR_SPACING` tiles, bar the
  /// middle where the players stand.
  void addPillars(GameSetup& setup) {
    for (int y = -4; y <= 4; ++y) {
      for (int x = -4; x <= 4; ++x) {
        if (x * x + y * y > 1) {
          const auto px = static_cast<float>(x * PILLAR_SPACING) + 0.5F;
          const auto py = static_cast<float>(y * PILLAR_SPACING) + 0.5F;
          setup.obstacles.push_back({{px, py, 0}, {px + 1, py + 1, 2}});
        }
      }
    }
  }

  /// Whether an actor of the default radius at @p at would be inside any
  /// of @p setup's obstacles.
  bool blocked(const GameSetup& setup, Vec2 at) {
    for (const physics::CollisionBox& box : setup.obstacles) {
      if (physics::cylinderOverlapsBox({at, 0.5F, 0.0F, 1.0F}, box)) {
        return true;
      }
    }
    return false;
  }

  /// A spot in the arena's outer ring, between 12 tiles out and the walls.
  Vec2 ringSpot(Pcg32& rng) {
    const math::SinCos way =
        math::sinCosDegrees(static_cast<float>(rng.nextBelow(360)));
    const float reach = 12.0F + (ARENA_HALF - 14.0F) * rng.nextUnitFloat();
    return {way.cos * reach, way.sin * reach};
  }

}  // namespace

GameSetup hordeSetup(uint32_t actors) {
  GameSetup setup;
  setup.seed = 2000;
  setup.player_count = 4;
  setup.spawns = {
      {{-1.5F, -1.5F, 0}, {1.5F, -1.5F, 0}, {-1.5F, 1.5F, 0}, {1.5F, 1.5F, 0}}};
  addWalls(setup);
  addPillars(setup);
  const EnemyDefinition swarmer = hordeContent().enemies.front();
  Pcg32 rng(7, 7);
  while (setup.actors.size() < actors) {
    const Vec2 at = ringSpot(rng);
    if (!blocked(setup, at)) {
      setup.actors.push_back(makeEnemySpawn(swarmer, {at.x, at.y, 0}, 0.0F));
    }
  }
  return setup;
}

GameContent hordeContent() {
  GameContent content;
  content.behaviors.push_back(resolveBehavior({}, "chase"));
  content.behaviors.back().senses.sight_range = 64.0F;
  content.behaviors.back().senses.view_degrees = 360.0F;
  content.enemies.push_back(
      {.id = "swarmer", .name = "Swarmer", .behavior = "chase"});
  return content;
}

sim::TickInput hordeInput(uint64_t tick) {
  sim::TickInput input;
  for (uint64_t slot = 0; slot < 4; ++slot) {
    const auto degrees = static_cast<float>((tick * (2 + slot)) % 360);
    const math::SinCos way = math::sinCosDegrees(degrees);
    input.players[slot].move_x = input::quantizeInputAxis(way.cos);
    input.players[slot].move_y = input::quantizeInputAxis(way.sin);
    input.players[slot].buttons =
        (tick + slot * 50) % 240 < 10 ? input::INPUT_BUTTON_FIRE : 0;
  }
  return input;
}

}  // namespace eng::game::test
