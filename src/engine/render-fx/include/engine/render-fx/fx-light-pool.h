#pragma once

/// @file fx-light-pool.h
/// @brief Every flash still shining, and the lights they give the scene.
/// @par Threading
/// Main-thread-only.

#include <cstddef>
#include <cstdint>
#include <engine/math/vec3.h>
#include <engine/render-fx/fx-flash.h>
#include <engine/render-mesh/mesh-light.h>
#include <vector>

namespace eng {

/// Flashes a pool holds at once unless it is given another number.
inline constexpr uint32_t FX_LIGHT_CAPACITY = 64;

/// Every flash still shining, packed as `FxParticlePool` is. A flash past
/// the capacity takes the place of the one nearest its end, since a new
/// flash is worth more to the scene than the last glimmer of an old one.
struct FxLightPool {
  /// A pool with room for @p capacity flashes.
  explicit FxLightPool(uint32_t capacity = FX_LIGHT_CAPACITY);

  /// How many flashes are shining.
  uint32_t live = 0;
  /// Where each shines from, in tiles.
  std::vector<Vec3> position;
  /// What each is.
  std::vector<FxFlash> flash;
  /// Seconds each has shone.
  std::vector<float> age;
};

/// Start @p flash shining at @p at. A flash of no intensity, range or life
/// is ignored.
void emitFxFlash(FxLightPool& pool, const FxFlash& flash, Vec3 at);

/// Age every flash by @p seconds, dropping those that have died away.
void stepFxLights(FxLightPool& pool, float seconds);

/// Drop every flash.
void clearFxLights(FxLightPool& pool);

/// The live flash at @p index as the mesh shader lights with it now: its
/// intensity falls with the square of what is left of its life, so it
/// flares and is quickly gone.
[[nodiscard]] MeshLight fxLightAt(const FxLightPool& pool, uint32_t index);

/// Append to @p out the @p room brightest flashes as they shine now,
/// brightest first — the ones worth the few light slots a mesh draw has.
void appendBrightestFxLights(const FxLightPool& pool, size_t room,
                             std::vector<MeshLight>& out);

}  // namespace eng
