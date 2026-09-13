#include <algorithm>
#include <engine/render-fx/fx-light-pool.h>

namespace eng {

namespace {

  /// How far through its life the flash at @p i is, 0 to 1.
  float progress(const FxLightPool& pool, uint32_t i) {
    return std::clamp(pool.age[i] / pool.flash[i].life, 0.0f, 1.0f);
  }

  /// Where a new flash goes: the next free place, or — when there is none —
  /// the place of the one nearest its end.
  uint32_t slotFor(FxLightPool& pool) {
    if (pool.live < pool.position.size()) {
      return pool.live++;
    }
    uint32_t oldest = 0;
    for (uint32_t i = 1; i < pool.live; ++i) {
      if (progress(pool, i) > progress(pool, oldest)) {
        oldest = i;
      }
    }
    return oldest;
  }

  /// Move the last live flash into @p i's place, and drop the last.
  void removeAt(FxLightPool& pool, uint32_t i) {
    const uint32_t last = --pool.live;
    pool.position[i] = pool.position[last];
    pool.flash[i] = pool.flash[last];
    pool.age[i] = pool.age[last];
  }

}  // namespace

FxLightPool::FxLightPool(uint32_t capacity)
  : position(capacity), flash(capacity), age(capacity) {}

void emitFxFlash(FxLightPool& pool, const FxFlash& flash, Vec3 at) {
  if (flash.intensity <= 0.0f || flash.range <= 0.0f || flash.life <= 0.0f ||
      pool.position.empty()) {
    return;
  }
  const uint32_t i = slotFor(pool);
  pool.position[i] = at;
  pool.flash[i] = flash;
  pool.age[i] = 0.0f;
}

void stepFxLights(FxLightPool& pool, float seconds) {
  uint32_t i = 0;
  while (i < pool.live) {
    pool.age[i] += seconds;
    if (pool.age[i] >= pool.flash[i].life) {
      removeAt(pool, i);
    } else {
      ++i;
    }
  }
}

void clearFxLights(FxLightPool& pool) {
  pool.live = 0;
}

MeshLight fxLightAt(const FxLightPool& pool, uint32_t index) {
  const FxFlash& flash = pool.flash[index];
  const float left = 1.0f - progress(pool, index);
  MeshLight light;
  light.position = pool.position[index];
  light.range = flash.range;
  light.intensity = flash.intensity * left * left;
  light.color = flash.color;
  light.kind = MESH_LIGHT_POINT;
  return light;
}

void appendBrightestFxLights(const FxLightPool& pool, size_t room,
                             std::vector<MeshLight>& out) {
  const size_t first = out.size();
  for (uint32_t i = 0; i < pool.live; ++i) {
    out.push_back(fxLightAt(pool, i));
  }
  const auto begin = out.begin() + static_cast<std::ptrdiff_t>(first);
  const size_t keep = std::min(room, out.size() - first);
  std::partial_sort(begin, begin + static_cast<std::ptrdiff_t>(keep), out.end(),
                    [](const MeshLight& a, const MeshLight& b) {
                      return a.intensity > b.intensity;
                    });
  out.resize(first + keep);
}

}  // namespace eng
