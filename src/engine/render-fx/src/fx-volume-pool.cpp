#include <algorithm>
#include <engine/render-fx/fx-volume-pool.h>

namespace eng {

namespace {

  /// The range a cloud's seed is drawn from: far enough apart that two
  /// clouds never show the same noise, small enough to keep the field's
  /// coordinates precise.
  constexpr float SEED_RANGE = 64.0f;

  /// Move the last live cloud into @p i's place, and drop the last.
  void removeAt(FxVolumePool& pool, uint32_t i) {
    const uint32_t last = --pool.live;
    pool.position[i] = pool.position[last];
    pool.look[i] = pool.look[last];
    pool.age[i] = pool.age[last];
    pool.seed[i] = pool.seed[last];
    pool.scale[i] = pool.scale[last];
  }

}  // namespace

FxVolumePool::FxVolumePool(uint32_t capacity)
  : position(capacity), look(capacity), age(capacity), seed(capacity),
    scale(capacity) {}

bool emitFxVolume(FxVolumePool& pool, const FxVolume& volume,
                  const FxEmit& emit, Pcg32& rng) {
  if (pool.live >= static_cast<uint32_t>(pool.position.size())) {
    return false;
  }
  const uint32_t i = pool.live++;
  pool.position[i] = emit.at;
  pool.look[i] = volume;
  pool.look[i].life = std::max(volume.life, 1e-3f);
  pool.age[i] = 0.0f;
  pool.seed[i] = rng.nextUnitFloat() * SEED_RANGE;
  pool.scale[i] = emit.scale;
  return true;
}

void stepFxVolumes(FxVolumePool& pool, float seconds) {
  uint32_t i = 0;
  while (i < pool.live) {
    pool.position[i].z += pool.look[i].rise * pool.scale[i] * seconds;
    pool.age[i] += seconds;
    if (pool.age[i] >= pool.look[i].life) {
      removeAt(pool, i);
    } else {
      ++i;
    }
  }
}

void clearFxVolumes(FxVolumePool& pool) {
  pool.live = 0;
}

float fxVolumeProgress(const FxVolumePool& pool, uint32_t index) {
  return std::clamp(pool.age[index] / pool.look[index].life, 0.0f, 1.0f);
}

float fxVolumeDensity(const FxVolumePool& pool, uint32_t index) {
  const float t = fxVolumeProgress(pool, index);
  const float swelling = std::min(t / FX_VOLUME_FADE_IN, 1.0f);
  return pool.look[index].density * swelling * (1.0f - t);
}

Vec3 fxVolumeExtents(const FxVolumePool& pool, uint32_t index) {
  const FxVolume& look = pool.look[index];
  const float grown = look.growth * pool.age[index];
  const float scale = pool.scale[index];
  return {(look.radius + grown) * scale, (look.radius + grown) * scale,
          (look.height + grown) * scale};
}

}  // namespace eng
