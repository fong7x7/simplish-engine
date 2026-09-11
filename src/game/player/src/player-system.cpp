#include <algorithm>
#include <engine/input/player-input-builder.h>
#include <engine/physics/cylinder-collision.h>
#include <engine/sim/slot-move.h>
#include <game/content/character-lookup.h>
#include <game/player/player-system.h>
#include <span>

namespace eng::game {

namespace {

  /// A quantised axis as the fraction of full scale it stands for.
  float axisFraction(int16_t axis) {
    return static_cast<float>(axis) / static_cast<float>(input::INPUT_AXIS_MAX);
  }

  /// Move the player at dense index @p i by @p input.
  void moveOne(PlayerPool& pool, uint32_t i, const sim::PlayerInput& input) {
    const float speed = pool.move_speed[i];
    pool.position[i].x += axisFraction(input.move_x) * speed;
    pool.position[i].y += axisFraction(input.move_y) * speed;
    if (input.aim_x != 0 || input.aim_y != 0) {
      pool.aim[i] = {axisFraction(input.aim_x), axisFraction(input.aim_y)};
    }
  }

  /// Push the player at dense index @p i out of every obstacle they are in.
  void collideOne(PlayerPool& pool, uint32_t i,
                  std::span<const physics::CollisionBox> obstacles) {
    Vec3& at = pool.position[i];
    const Vec2 clear = physics::resolveCylinderAgainstBoxes(
        {{at.x, at.y}, PLAYER_RADIUS_TILES, at.z, PLAYER_HEIGHT_TILES},
        obstacles);
    at.x = clear.x;
    at.y = clear.y;
  }

  /// Give the player at dense index @p i a full bar of @p health, up.
  void startHealth(PlayerPool& pool, uint32_t i, uint16_t health) {
    pool.health[i] = health;
    pool.max_health[i] = health;
    pool.hurt_until[i] = 0;
    pool.downed[i] = 0;
    pool.downed_since[i] = 0;
    pool.revive_ticks[i] = 0;
    pool.out[i] = 0;
  }

  /// Fold the health and downed fields of the first @p live players in.
  void hashDowned(const PlayerPool& pool, sim::StateHasher& hasher,
                  uint32_t live) {
    hasher.addSpan(std::span<const uint16_t>(pool.max_health).first(live));
    hasher.addSpan(std::span<const uint64_t>(pool.hurt_until).first(live));
    hasher.addSpan(std::span<const uint8_t>(pool.downed).first(live));
    hasher.addSpan(std::span<const uint64_t>(pool.downed_since).first(live));
    hasher.addSpan(std::span<const uint32_t>(pool.revive_ticks).first(live));
    hasher.addSpan(std::span<const uint8_t>(pool.out).first(live));
  }

  /// Whether a teammate who is up stands within reach of player @p i.
  bool teammateBeside(const PlayerPool& pool, uint32_t i) {
    constexpr float REACH =
        PLAYER_REVIVE_REACH_TILES * PLAYER_REVIVE_REACH_TILES;
    const Vec2 at{pool.position[i].x, pool.position[i].y};
    for (uint32_t j = 0; j < pool.slots.size(); ++j) {
      const Vec2 other{pool.position[j].x, pool.position[j].y};
      if (j != i && playerIsUp(pool, j) &&
          Vec2::distanceSquared(at, other) <= REACH) {
        return true;
      }
    }
    return false;
  }

  /// Whether any player but @p i is up to revive them.
  bool anyTeammateUp(const PlayerPool& pool, uint32_t i) {
    for (uint32_t j = 0; j < pool.slots.size(); ++j) {
      if (j != i && playerIsUp(pool, j)) {
        return true;
      }
    }
    return false;
  }

  /// Bring the downed player @p i back up.
  void revive(PlayerPool& pool, uint32_t i, uint64_t tick) {
    pool.downed[i] = 0;
    pool.revive_ticks[i] = 0;
    pool.health[i] = std::min(PLAYER_REVIVED_HEALTH, pool.max_health[i]);
    pool.hurt_until[i] = tick + PLAYER_HURT_GRACE_TICKS;
  }

  /// One downed player's tick: a teammate by them revives them, and the
  /// window running out — or nobody left to come — puts them out.
  void updateOne(PlayerPool& pool, uint32_t i, uint64_t tick) {
    pool.revive_ticks[i] =
        teammateBeside(pool, i) ? pool.revive_ticks[i] + 1 : 0;
    if (pool.revive_ticks[i] >= PLAYER_REVIVE_TICKS) {
      revive(pool, i, tick);
    } else if (tick - pool.downed_since[i] >= PLAYER_DOWNED_WINDOW_TICKS ||
               !anyTeammateUp(pool, i)) {
      pool.downed[i] = 0;
      pool.out[i] = 1;
    }
  }

}  // namespace

bool playerIsUp(const PlayerPool& pool, uint32_t index) {
  return pool.downed[index] == 0 && pool.out[index] == 0;
}

void hurtPlayer(PlayerPool& pool, uint32_t index, uint16_t amount,
                uint64_t tick) {
  if (!playerIsUp(pool, index) || tick < pool.hurt_until[index]) {
    return;
  }
  pool.health[index] -= std::min(amount, pool.health[index]);
  pool.hurt_until[index] = tick + PLAYER_HURT_GRACE_TICKS;
  if (pool.health[index] == 0) {
    pool.downed[index] = 1;
    pool.downed_since[index] = tick;
    pool.revive_ticks[index] = 0;
  }
}

void updateDownedPlayers(PlayerPool& pool, uint64_t tick) {
  for (uint32_t i = 0; i < pool.slots.size(); ++i) {
    if (pool.downed[i] != 0) {
      updateOne(pool, i, tick);
    }
  }
}

std::optional<sim::EntityHandle>
spawnPlayer(PlayerPool& pool, uint8_t input_slot, Vec3 at,
            const CharacterDefinition& character) {
  const std::optional<sim::EntityHandle> handle = pool.slots.spawn();
  if (!handle) {
    return std::nullopt;
  }
  // `spawn` always places a new entity at the end of the dense range.
  const uint32_t i = pool.slots.size() - 1U;
  pool.position[i] = at;
  pool.aim[i] = {1.0F, 0.0F};
  pool.input_slot[i] = input_slot;
  pool.move_speed[i] = characterSpeedPerTick(character);
  startHealth(pool, i, character.health);
  return handle;
}

void movePlayers(PlayerPool& pool, const sim::TickInput& input,
                 std::span<const physics::CollisionBox> obstacles) {
  for (uint32_t i = 0; i < pool.slots.size(); ++i) {
    const uint8_t slot = pool.input_slot[i];
    if (slot < input.players.size() && playerIsUp(pool, i)) {
      moveOne(pool, i, input.players[slot]);
    }
    collideOne(pool, i, obstacles);
  }
}

void compactPlayers(PlayerPool& pool) {
  const auto moves = pool.slots.compact();
  sim::applySlotMoves(moves, pool.position);
  sim::applySlotMoves(moves, pool.aim);
  sim::applySlotMoves(moves, pool.input_slot);
  sim::applySlotMoves(moves, pool.move_speed);
  sim::applySlotMoves(moves, pool.health);
  sim::applySlotMoves(moves, pool.max_health);
  sim::applySlotMoves(moves, pool.hurt_until);
  sim::applySlotMoves(moves, pool.downed);
  sim::applySlotMoves(moves, pool.downed_since);
  sim::applySlotMoves(moves, pool.revive_ticks);
  sim::applySlotMoves(moves, pool.out);
}

void hashPlayers(const PlayerPool& pool, sim::StateHasher& hasher) {
  const uint32_t live = pool.slots.size();
  pool.slots.hashInto(hasher);
  hasher.addSpan(std::span<const Vec3>(pool.position).first(live));
  hasher.addSpan(std::span<const Vec2>(pool.aim).first(live));
  hasher.addSpan(std::span<const uint8_t>(pool.input_slot).first(live));
  hasher.addSpan(std::span<const float>(pool.move_speed).first(live));
  hasher.addSpan(std::span<const uint16_t>(pool.health).first(live));
  hashDowned(pool, hasher, live);
}

}  // namespace eng::game
