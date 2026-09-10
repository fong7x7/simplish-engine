#include <engine/input/player-input-builder.h>
#include <engine/physics/cylinder-collision.h>
#include <engine/sim/slot-move.h>
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
    pool.position[i].x +=
        axisFraction(input.move_x) * PLAYER_SPEED_TILES_PER_TICK;
    pool.position[i].y +=
        axisFraction(input.move_y) * PLAYER_SPEED_TILES_PER_TICK;
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

}  // namespace

std::optional<sim::EntityHandle> spawnPlayer(PlayerPool& pool,
                                             uint8_t input_slot, Vec3 at) {
  const std::optional<sim::EntityHandle> handle = pool.slots.spawn();
  if (!handle) {
    return std::nullopt;
  }
  // `spawn` always places a new entity at the end of the dense range.
  const uint32_t i = pool.slots.size() - 1U;
  pool.position[i] = at;
  pool.aim[i] = {1.0F, 0.0F};
  pool.input_slot[i] = input_slot;
  return handle;
}

void movePlayers(PlayerPool& pool, const sim::TickInput& input,
                 std::span<const physics::CollisionBox> obstacles) {
  for (uint32_t i = 0; i < pool.slots.size(); ++i) {
    const uint8_t slot = pool.input_slot[i];
    if (slot < input.players.size()) {
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
}

void hashPlayers(const PlayerPool& pool, sim::StateHasher& hasher) {
  const uint32_t live = pool.slots.size();
  pool.slots.hashInto(hasher);
  hasher.addSpan(std::span<const Vec3>(pool.position).first(live));
  hasher.addSpan(std::span<const Vec2>(pool.aim).first(live));
  hasher.addSpan(std::span<const uint8_t>(pool.input_slot).first(live));
}

}  // namespace eng::game
