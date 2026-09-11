#include <engine/input/player-input-builder.h>
#include <game/player/player-system.h>
#include <game/world/stand-in-input.h>
#include <optional>
#include <tuple>
#include <utility>

namespace eng::game {

namespace {

  /// @p v on the floor.
  Vec2 flat(Vec3 v) {
    return {v.x, v.y};
  }

  /// The dense index of the player in input slot @p slot, if any.
  std::optional<uint32_t> playerInSlot(const PlayerPool& players,
                                       uint8_t slot) {
    for (uint32_t p = 0; p < players.slots.size(); ++p) {
      if (players.input_slot[p] == slot) {
        return p;
      }
    }
    return std::nullopt;
  }

  /// Where the nearest live hostile actor within @p range of @p at stands.
  std::optional<Vec2> nearestHostile(const ActorPool& actors, Vec2 at,
                                     float range) {
    std::optional<Vec2> best;
    float best_d2 = range * range;
    for (uint32_t i = 0; i < actors.slots.size(); ++i) {
      const Vec2 there = flat(actors.position[i]);
      const float d2 = Vec2::distanceSquared(at, there);
      if (actors.faction[i] == Faction::HOSTILE && actors.health[i] != 0 &&
          d2 <= best_d2) {
        best = there;
        best_d2 = d2;
      }
    }
    return best;
  }

  /// Where the first player but @p self that @p wanted picks stands.
  std::optional<Vec2> teammate(const PlayerPool& players, uint32_t self,
                               bool (*wanted)(const PlayerPool&, uint32_t)) {
    for (uint32_t p = 0; p < players.slots.size(); ++p) {
      if (p != self && wanted(players, p)) {
        return flat(players.position[p]);
      }
    }
    return std::nullopt;
  }

  /// Which way a stand-in at @p at wants to go.
  Vec2 wayToGo(const GameWorld& world, uint32_t self, Vec2 at) {
    const PlayerPool& players = world.players();
    const auto down =
        teammate(players, self, [](const PlayerPool& pool, uint32_t p) {
          return pool.downed[p] != 0;
        });
    if (down) {
      return *down - at;
    }
    if (const auto hostile =
            nearestHostile(world.actors(), at, STAND_IN_WARY_TILES)) {
      return at - *hostile;
    }
    const auto lead = teammate(players, self, playerIsUp);
    constexpr float FOLLOW = STAND_IN_FOLLOW_TILES * STAND_IN_FOLLOW_TILES;
    return lead && Vec2::distanceSquared(*lead, at) > FOLLOW ? *lead - at
                                                             : Vec2{};
  }

  /// @p v, a direction, as a stick: full over it, and no more.
  std::pair<int16_t, int16_t> stick(Vec2 v) {
    const Vec2 unit = Vec2::normalize(v);
    return {input::quantizeInputAxis(unit.x), input::quantizeInputAxis(unit.y)};
  }

}  // namespace

sim::PlayerInput standInInput(const GameWorld& world, uint8_t slot) {
  const PlayerPool& players = world.players();
  const auto self = playerInSlot(players, slot);
  if (!self || !playerIsUp(players, *self)) {
    return {};
  }
  const Vec2 at = flat(players.position[*self]);
  sim::PlayerInput input;
  std::tie(input.move_x, input.move_y) = stick(wayToGo(world, *self, at));
  if (const auto hostile =
          nearestHostile(world.actors(), at, STAND_IN_AIM_TILES)) {
    std::tie(input.aim_x, input.aim_y) = stick(*hostile - at);
  }
  return input;
}

}  // namespace eng::game
