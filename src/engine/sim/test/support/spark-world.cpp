#include "support/spark-world.h"

#include <engine/sim/slot-move.h>
#include <span>

namespace eng::sim::testing {

namespace {

  /// Stream ids, one per system, as a game would name them.
  constexpr uint64_t SPAWN_STREAM = 1;
  constexpr uint64_t FX_STREAM = 2;

  /// Ticks a spark lives.
  constexpr uint32_t LIFETIME_TICKS = 45;

  /// Sparks further than this from the origin on either axis are removed.
  constexpr float WORLD_EXTENT = 200.0F;

  /// Full-scale input axis value.
  constexpr float AXIS_SCALE = 32767.0F;

  bool outOfBounds(Vec2 position) {
    return position.x > WORLD_EXTENT || position.x < -WORLD_EXTENT ||
           position.y > WORLD_EXTENT || position.y < -WORLD_EXTENT;
  }

}  // namespace

SparkWorld::SparkWorld(uint64_t seed, uint32_t capacity)
  : slots_(capacity), position_(capacity), velocity_(capacity), age_(capacity),
    spawn_rng_(seed, SPAWN_STREAM), fx_rng_(seed, FX_STREAM) {}

void SparkWorld::hashState(TickHashBuilder& builder) const {
  StateHasher& sparks = builder.section("sparks");
  slots_.hashInto(sparks);
  sparks.addSpan(std::span<const Vec2>(position_).first(slots_.size()));
  sparks.addSpan(std::span<const Vec2>(velocity_).first(slots_.size()));
  sparks.addSpan(std::span<const uint32_t>(age_).first(slots_.size()));
  builder.section("spawn_rng").add(spawn_rng_.state());
}

void SparkWorld::playerControl(const TickContext& context) {
  for (std::size_t player = 0; player < MAX_PLAYERS; ++player) {
    const PlayerInput& input = context.input.players[player];
    if ((input.buttons & 1U) == 0U) {
      continue;
    }
    const Vec2 origin{static_cast<float>(player) * 10.0F, 0.0F};
    const Vec2 aim{static_cast<float>(input.aim_x) / AXIS_SCALE,
                   static_cast<float>(input.aim_y) / AXIS_SCALE};
    spawn(origin, aim * 2.0F);
  }
}

void SparkWorld::director([[maybe_unused]] const TickContext& context) {
  for (uint32_t i = 0; i < cosmetic_draws_; ++i) {
    (void)fx_rng_.next();
  }
  if (spawn_rng_.nextBelow(3U) != 0U) {
    return;
  }
  const Vec2 position{spawn_rng_.nextUnitFloat() * 100.0F - 50.0F,
                      spawn_rng_.nextUnitFloat() * 100.0F - 50.0F};
  const Vec2 velocity{spawn_rng_.nextUnitFloat() - 0.5F,
                      spawn_rng_.nextUnitFloat() - 0.5F};
  spawn(position, velocity);
}

void SparkWorld::spawn(Vec2 position, Vec2 velocity) {
  const auto handle = slots_.spawn();
  if (!handle) {
    return;
  }
  const uint32_t index = *slots_.denseIndex(*handle);
  position_[index] = position;
  velocity_[index] = velocity;
  age_[index] = 0;
}

void SparkWorld::projectiles([[maybe_unused]] const TickContext& context) {
  for (uint32_t i = 0; i < slots_.size(); ++i) {
    position_[i] = position_[i] + velocity_[i];
    ++age_[i];
  }
}

void SparkWorld::damage([[maybe_unused]] const TickContext& context) {
  for (uint32_t i = 0; i < slots_.size(); ++i) {
    if (age_[i] > LIFETIME_TICKS || outOfBounds(position_[i])) {
      (void)slots_.destroy(slots_.handleAt(i));
    }
  }
}

void SparkWorld::compaction([[maybe_unused]] const TickContext& context) {
  const auto moves = slots_.compact();
  applySlotMoves(moves, position_);
  applySlotMoves(moves, velocity_);
  applySlotMoves(moves, age_);
}

}  // namespace eng::sim::testing
