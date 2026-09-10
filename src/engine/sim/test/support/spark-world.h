#pragma once

/// @file spark-world.h
/// @brief A tiny game for the sim tests, shaped like a real one.
/// @par Threading
/// Main-thread-only.

#include <cstdint>
#include <engine/core/pcg32.h>
#include <engine/math/vec2.h>
#include <engine/sim/entity-slots.h>
#include <engine/sim/simulation-systems.h>
#include <vector>

namespace eng::sim::testing {

/// A `SimulationSystems` with the moving parts a real game has, so the
/// determinism and replay tests exercise the real path: one SoA pool of
/// sparks, spawned by player input and by an RNG stream, integrated with
/// float math, expired through deferred destruction and compaction — plus a
/// cosmetic RNG stream that must stay out of the hash.
class SparkWorld final : public SimulationSystems {
public:
  /// A world seeded with `seed` holding at most `capacity` sparks.
  SparkWorld(uint64_t seed, uint32_t capacity);

  /// Each player holding button 0 fires a spark along their aim.
  void playerControl(const TickContext& context) override;
  /// Moves and ages every spark.
  void projectiles(const TickContext& context) override;
  /// Marks old and out-of-bounds sparks for destruction.
  void damage(const TickContext& context) override;
  /// One tick in three, a spark appears somewhere random.
  void director(const TickContext& context) override;
  /// Destroys what `damage` marked and moves the fields to match.
  void compaction(const TickContext& context) override;
  /// The spark pool and the spawn stream; never the cosmetic stream.
  void hashState(TickHashBuilder& builder) const override;

  /// Draws `count` numbers from the cosmetic stream every tick, as a
  /// particle system would.
  void setCosmeticDrawsPerTick(uint32_t count) { cosmetic_draws_ = count; }

  /// Live sparks.
  [[nodiscard]] uint32_t sparkCount() const { return slots_.size(); }

private:
  /// Adds a spark, or does nothing when the pool is full.
  void spawn(Vec2 position, Vec2 velocity);

  /// Handle bookkeeping for the spark pool.
  EntitySlots slots_;
  /// Spark positions, by dense index.
  std::vector<Vec2> position_;
  /// Spark velocities, by dense index.
  std::vector<Vec2> velocity_;
  /// Ticks each spark has lived, by dense index.
  std::vector<uint32_t> age_;
  /// Simulation stream: decides ambient spawns. Hashed.
  Pcg32 spawn_rng_;
  /// Cosmetic stream: drawn from, never hashed.
  Pcg32 fx_rng_;
  /// Cosmetic draws per tick.
  uint32_t cosmetic_draws_ = 0;
};

}  // namespace eng::sim::testing
