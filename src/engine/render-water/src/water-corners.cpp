#include <cmath>
#include <engine/render-water/water-corners.h>
#include <engine/render-water/water-depth.h>

namespace eng {

namespace {

  /// A sample weighted by @p weight, to be summed with others.
  WaterSample weighted(const WaterSample& sample, float weight) {
    return {sample.depth * weight, sample.color * weight,
            sample.opacity * weight, sample.flow * weight};
  }

  /// @p a and @p b added field by field.
  WaterSample plus(const WaterSample& a, const WaterSample& b) {
    return {a.depth + b.depth, a.color + b.color, a.opacity + b.opacity,
            a.flow + b.flow};
  }

  /// The water on @p cell as numbers, or nothing — depth 0 — when dry.
  WaterSample cellSample(const WaterLayer& layer, GroundCell cell) {
    const WaterCell water = waterCellAt(layer, cell);
    if (water.depth == 0) {
      return {};
    }
    return {waterDepthTiles(water.depth),
            Vec3{static_cast<float>(water.red), static_cast<float>(water.green),
                 static_cast<float>(water.blue)} *
                (1.0f / 255.0f),
            static_cast<float>(water.opacity) / 255.0f, waterCellFlow(water)};
  }

  /// The mean of the water cells meeting at corner @p corner — the cell's
  /// own south-west corner — or dry when none are water.
  WaterSample cornerSample(const WaterLayer& layer, GroundCell corner) {
    WaterSample sum{};
    float count = 0.0f;
    for (int32_t dy = -1; dy <= 0; ++dy) {
      for (int32_t dx = -1; dx <= 0; ++dx) {
        const WaterSample one =
            cellSample(layer, {corner.x + dx, corner.y + dy});
        sum = plus(sum, one);
        count += one.depth > 0.0f ? 1.0f : 0.0f;
      }
    }
    return count > 0.0f ? weighted(sum, 1.0f / count) : WaterSample{};
  }

  /// The sample at corner (@p x, @p y) of @p corners, dry outside.
  WaterSample cornerAt(const WaterCorners& corners, int32_t x, int32_t y) {
    if (x < 0 || y < 0 || x >= corners.width || y >= corners.height) {
      return {};
    }
    return corners
        .samples[static_cast<size_t>(y) * static_cast<size_t>(corners.width) +
                 static_cast<size_t>(x)];
  }

  /// @p sample's colour and opacity weighted by its depth, as the blend
  /// sums them.
  WaterSample byDepth(const WaterSample& sample, float weight) {
    return {sample.depth * weight, sample.color * (sample.depth * weight),
            sample.opacity * sample.depth * weight,
            sample.flow * (sample.depth * weight)};
  }

}  // namespace

WaterCorners makeWaterCorners(const WaterLayer& layer, GroundRect cells) {
  WaterCorners corners{
      {cells.x, cells.y}, cells.width + 1, cells.height + 1, {}};
  corners.samples.reserve(static_cast<size_t>(corners.width) *
                          static_cast<size_t>(corners.height));
  for (int32_t y = 0; y < corners.height; ++y) {
    for (int32_t x = 0; x < corners.width; ++x) {
      corners.samples.push_back(
          cornerSample(layer, {corners.origin.x + x, corners.origin.y + y}));
    }
  }
  return corners;
}

WaterSample waterSampleAt(const WaterCorners& corners, Vec2 at) {
  const float fx = at.x - static_cast<float>(corners.origin.x);
  const float fy = at.y - static_cast<float>(corners.origin.y);
  const float tx = fx - std::floor(fx);
  const float ty = fy - std::floor(fy);
  const auto cx = static_cast<int32_t>(std::floor(fx));
  const auto cy = static_cast<int32_t>(std::floor(fy));
  WaterSample sum = byDepth(cornerAt(corners, cx, cy), (1 - tx) * (1 - ty));
  sum = plus(sum, byDepth(cornerAt(corners, cx + 1, cy), tx * (1 - ty)));
  sum = plus(sum, byDepth(cornerAt(corners, cx, cy + 1), (1 - tx) * ty));
  sum = plus(sum, byDepth(cornerAt(corners, cx + 1, cy + 1), tx * ty));
  if (sum.depth <= 0.0f) {
    return {};
  }
  return {sum.depth, sum.color * (1.0f / sum.depth), sum.opacity / sum.depth,
          sum.flow * (1.0f / sum.depth)};
}

}  // namespace eng
