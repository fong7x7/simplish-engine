#pragma once

/// @file simulation.h
/// @brief Advances the simulation one fixed tick at a time.
/// @par Threading
/// Main-thread-only.

#include <cstdint>
#include <engine/sim/simulation-systems.h>
#include <engine/sim/tick-hash.h>
#include <engine/sim/tick-input.h>
#include <engine/sim/tick-result.h>

namespace eng::sim {

/// Whether each tick ends with a state hash. Debug and CI builds hash every
/// tick; the cost is one pass over every hashed field.
enum class TickHashing : uint8_t {
  OFF,  ///< No hash; `TickResult::hash` is empty
  ON,   ///< Every tick is hashed
};

/// The deterministic tick (ADR-002). A `Simulation` is a pure function of
/// the state its systems start in and the inputs it is stepped with: step
/// two of them from the same state with the same inputs and their tick
/// hashes match on every tick, on every platform.
///
/// It owns only the tick counter and the phase order. State lives in the
/// game's `SimulationSystems`; time is the tick count, never a clock; and
/// the caller decides when to step — a `FixedStepClock` for live play, a
/// replay's input list for playback, as fast as possible for a CI run.
class Simulation {
public:
  /// A simulation over `systems`, which must outlive it, starting at tick 0.
  Simulation(SimulationSystems& systems, TickHashing hashing);

  /// Runs one tick on `input`: every phase of `SimulationSystems` in order,
  /// then the tick hash when hashing is on.
  TickResult step(const TickInput& input);

  /// The tick the next `step` will simulate.
  [[nodiscard]] uint64_t nextTick() const { return next_tick_; }

  /// Whether ticks are hashed.
  [[nodiscard]] TickHashing hashing() const { return hashing_; }

private:
  /// Calls the game's phases in §4.1 order.
  void runPhases(const TickContext& context);

  /// The hash of the systems' state at the end of `tick`.
  [[nodiscard]] TickHash hashTick(uint64_t tick) const;

  /// The game's phases and state. Never null.
  SimulationSystems* systems_;
  /// Whether `step` hashes.
  TickHashing hashing_;
  /// The tick the next `step` simulates.
  uint64_t next_tick_ = 0;
};

}  // namespace eng::sim
