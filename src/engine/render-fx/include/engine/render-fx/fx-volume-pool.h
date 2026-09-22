#pragma once

/// @file fx-volume-pool.h
/// @brief Every cloud of smoke alive, and how one is started and aged.
/// @par Threading
/// Main-thread-only.

#include <cstdint>
#include <engine/core/pcg32.h>
#include <engine/math/vec3.h>
#include <engine/render-fx/fx-emit.h>
#include <engine/render-fx/fx-volume.h>
#include <vector>

namespace eng {

/// How many clouds a world holds at once. A march a pixel is dear enough
/// that this is a handful, not a crowd: past it, a new cloud is dropped.
inline constexpr uint32_t FX_VOLUME_CAPACITY = 16;

/// Structure of arrays, the particle pool's shape exactly: `live` entries
/// are alive, each array is that long, and a dead one is filled by the
/// last.
struct FxVolumePool {
  /// A pool holding @p capacity clouds.
  explicit FxVolumePool(uint32_t capacity = FX_VOLUME_CAPACITY);

  /// Where each cloud's middle is, in tiles.
  std::vector<Vec3> position;
  /// How each looks and how long it lasts.
  std::vector<FxVolume> look;
  /// How long each has been alive, in seconds.
  std::vector<float> age;
  /// What each one's noise field is moved by, so no two are alike.
  std::vector<float> seed;
  /// What the emit scaled each one's extents and drift by.
  std::vector<float> scale;
  /// How many of the arrays' entries are alive.
  uint32_t live = 0;
};

/// Start @p volume at @p emit, seeded from @p rng. Returns false when the
/// pool is full, which drops the cloud rather than stealing one.
bool emitFxVolume(FxVolumePool& pool, const FxVolume& volume,
                  const FxEmit& emit, Pcg32& rng);

/// Drift and age every cloud by @p seconds, dropping those past their life.
void stepFxVolumes(FxVolumePool& pool, float seconds);

/// Drop every cloud at once.
void clearFxVolumes(FxVolumePool& pool);

/// How far through its life the cloud at @p index is, 0 to 1.
[[nodiscard]] float fxVolumeProgress(const FxVolumePool& pool, uint32_t index);

/// How thick the cloud at @p index is now: its density, faded in out of
/// nothing and thinned away to nothing again by the end.
[[nodiscard]] float fxVolumeDensity(const FxVolumePool& pool, uint32_t index);

/// Half the width, depth and height of the cloud at @p index now, in
/// tiles: what it was born with, scaled by its emit and grown since.
[[nodiscard]] Vec3 fxVolumeExtents(const FxVolumePool& pool, uint32_t index);

}  // namespace eng
