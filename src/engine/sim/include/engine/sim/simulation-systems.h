#pragma once

/// @file simulation-systems.h
/// @brief The game's side of a tick: its phases and its state hash.
/// @par Threading
/// Main-thread-only; called only from `Simulation::step`.

#include <engine/sim/tick-context.h>
#include <engine/sim/tick-hash-builder.h>

namespace eng::sim {

/// Implemented by the game to plug its systems into the tick. The engine
/// owns the order; the game owns what each phase does.
///
/// `Simulation::step` calls the phases below in the order they are declared,
/// which is the order of Engine REQUIREMENTS §4.1 steps 2–8 and part of the
/// determinism contract (ADR-002). Step 1, draining the input queue, is the
/// `TickInput` handed to `step`; step 9 is `hashState`. A game overrides the
/// phases it has, and the rest do nothing.
///
/// One virtual call per phase per tick — nine a tick, not one per entity.
/// Inside a phase the game iterates its own pools directly (ADR-004).
class SimulationSystems {
public:
  virtual ~SimulationSystems() = default;

  /// Step 2 — player controllers.
  virtual void playerControl([[maybe_unused]] const TickContext& context) {}

  /// Step 3 — enemy AI and steering.
  virtual void enemyAi([[maybe_unused]] const TickContext& context) {}

  /// Step 4 — weapon fire resolution and projectile spawn.
  virtual void weaponFire([[maybe_unused]] const TickContext& context) {}

  /// Step 5 — projectile integration and collision.
  virtual void projectiles([[maybe_unused]] const TickContext& context) {}

  /// Step 6 — damage resolution and death.
  virtual void damage([[maybe_unused]] const TickContext& context) {}

  /// Step 7 — the spawn director.
  virtual void director([[maybe_unused]] const TickContext& context) {}

  /// Step 8 — deferred destruction and index compaction: every pool's
  /// `EntitySlots::compact`, with its moves applied to the pool's fields.
  virtual void compaction([[maybe_unused]] const TickContext& context) {}

  /// Step 9 — folds every piece of simulation state into `builder`, one
  /// section per subsystem. Runs after `compaction`, so pools are dense and
  /// nothing is pending destruction. Cosmetic state — FX RNG streams,
  /// particle positions — stays out, so visual variance cannot desync a
  /// session.
  virtual void hashState(TickHashBuilder& builder) const = 0;

  SimulationSystems(const SimulationSystems&) = delete;
  SimulationSystems& operator=(const SimulationSystems&) = delete;
  SimulationSystems(SimulationSystems&&) = delete;
  SimulationSystems& operator=(SimulationSystems&&) = delete;

protected:
  SimulationSystems() = default;
};

}  // namespace eng::sim
