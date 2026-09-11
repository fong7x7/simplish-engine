#include <algorithm>
#include <cmath>
#include <engine/physics/segment-queries.h>
#include <utility>

namespace eng::physics {

namespace {

  /// The fractions of a step along one axis, from @p start moving @p delta,
  /// between which it is inside [@p low, @p high]; nothing when never. A
  /// step not moving along the axis is inside all of it or none.
  std::optional<std::pair<float, float>> slab(float start, float delta,
                                              float low, float high) {
    if (delta == 0.0F) {
      return start >= low && start <= high
                 ? std::optional{std::pair{0.0F, 1.0F}}
                 : std::nullopt;
    }
    const float a = (low - start) / delta;
    const float b = (high - start) / delta;
    return std::pair{std::min(a, b), std::max(a, b)};
  }

}  // namespace

std::optional<float> sweepHitsBox(const SegmentSweep& sweep,
                                  const CollisionBox& box) {
  if (sweep.z < box.min.z - sweep.radius ||
      sweep.z > box.max.z + sweep.radius) {
    return std::nullopt;
  }
  const Vec2 delta = sweep.to - sweep.from;
  const auto x = slab(sweep.from.x, delta.x, box.min.x - sweep.radius,
                      box.max.x + sweep.radius);
  const auto y = slab(sweep.from.y, delta.y, box.min.y - sweep.radius,
                      box.max.y + sweep.radius);
  if (!x || !y) {
    return std::nullopt;
  }
  const float enter = std::max({x->first, y->first, 0.0F});
  const float leave = std::min({x->second, y->second, 1.0F});
  return enter <= leave ? std::optional{enter} : std::nullopt;
}

std::optional<float> sweepHitsCircle(const SegmentSweep& sweep, Vec2 center,
                                     float radius) {
  const float reach = radius + sweep.radius;
  const Vec2 start = sweep.from - center;
  const float c = Vec2::lengthSquared(start) - reach * reach;
  if (c <= 0.0F) {
    return 0.0F;
  }
  const Vec2 delta = sweep.to - sweep.from;
  const float a = Vec2::lengthSquared(delta);
  const float b = Vec2::dot(start, delta);
  const float discriminant = b * b - a * c;
  if (a == 0.0F || b >= 0.0F || discriminant < 0.0F) {
    return std::nullopt;
  }
  const float t = (-b - std::sqrt(discriminant)) / a;
  return t <= 1.0F ? std::optional{t} : std::nullopt;
}

}  // namespace eng::physics
