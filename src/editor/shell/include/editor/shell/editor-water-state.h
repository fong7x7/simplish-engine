#pragma once

/// @file editor-water-state.h
/// @brief The level's water, as far as the rest of the editor can see it.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <engine/render-water/water-fidelity.h>

namespace eng::editor {

/// What the viewport's water is doing, refreshed by the editor every frame
/// — a mirror, as `EditorEffectsState` is, so the agent API can read it
/// without holding the simulation.
/// @thread_safety Main-thread-only.
struct EditorWaterState {
  /// The fidelity it is simulated and drawn at.
  WaterFidelity fidelity = WATER_DEFAULT_FIDELITY;
  /// Whether a surface was drawn last frame: false with no water, and on a
  /// backend without a water pipeline, where water is not drawn at all.
  bool drawn = false;
  /// Samples along each side of a tile the ripples are simulated at: the
  /// fidelity's, or fewer when the water is too wide for the budget.
  uint32_t samples_per_tile = 0;
  /// How many samples are water.
  uint32_t wet_samples = 0;
  /// How much the water is moving (`waterFieldEnergy`): zero when still.
  double energy = 0.0;
  /// How many times anything has pushed it — a step through it, a shot or
  /// a blast landing in it — since the editor opened. Drizzle is not
  /// counted.
  uint64_t pushes = 0;
  /// How many splashes it has thrown up — a shot or a blast landing in
  /// it, a wader's feet — since the editor opened.
  uint64_t splashes = 0;
  /// How many placements stand in it for its ripples to go round.
  uint32_t obstacles = 0;
};

}  // namespace eng::editor
