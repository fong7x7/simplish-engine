#include <algorithm>
#include <editor/shell/editor-placement-pick.h>
#include <limits>
#include <utility>

namespace eng::editor {

namespace {

  /// The span of ray parameters still inside the box, narrowed one axis at
  /// a time.
  struct RaySpan {
    /// Where the box is first entered.
    float enter = std::numeric_limits<float>::lowest();
    /// Where it is last left.
    float exit = std::numeric_limits<float>::max();
  };

  /// One axis of the box, and where the ray crosses that axis.
  struct Slab {
    /// Where the ray starts on this axis.
    float origin = 0.0f;
    /// How fast the ray moves along it.
    float direction = 0.0f;
    /// The box's near edge on this axis.
    float lo = 0.0f;
    /// The box's far edge on this axis.
    float hi = 0.0f;
  };

  /// Narrow @p span to the part of the ray inside @p slab. Returns false
  /// once nothing is left, which means the ray misses.
  bool clipSlab(RaySpan& span, const Slab& slab) {
    const float origin = slab.origin;
    if (slab.direction == 0.0f) {
      // The ray never crosses this axis, so it is either inside the slab
      // for its whole length or outside it for all of it.
      return origin >= slab.lo && origin <= slab.hi;
    }
    float near_t = (slab.lo - origin) / slab.direction;
    float far_t = (slab.hi - origin) / slab.direction;
    if (near_t > far_t) {
      std::swap(near_t, far_t);
    }
    span.enter = std::max(span.enter, near_t);
    span.exit = std::min(span.exit, far_t);
    return span.enter <= span.exit;
  }

}  // namespace

std::optional<float> placementRayHit(const IsoView& view,
                                     const PlacementBounds& bounds,
                                     IsoPoint screen) {
  // Any world point that projects to the click will do as an origin, and
  // the ground plane is the one the viewport can already invert to.
  const WorldPoint origin = screenToWorld(view, screen);
  const WorldPoint ray = isoProjectionRay(view.axes);
  RaySpan span{};
  if (!clipSlab(span, {origin.x, ray.x, bounds.min.x, bounds.max.x}) ||
      !clipSlab(span, {origin.y, ray.y, bounds.min.y, bounds.max.y}) ||
      !clipSlab(span, {origin.z, ray.z, bounds.min.z, bounds.max.z})) {
    return std::nullopt;
  }
  // The near face is the one the viewer sees, and the far end of the span
  // is the near face: the ray runs toward the camera.
  return span.exit;
}

int pickPlacementMarker(const IsoView& view,
                        const std::vector<EditorPlacementMarker>& markers,
                        IsoPoint screen) {
  int picked = EDITOR_PLACEMENT_NONE;
  float nearest = 0.0f;
  for (size_t i = 0; i < markers.size(); ++i) {
    const std::optional<float> hit =
        placementRayHit(view, markers[i].bounds, screen);
    if (!hit.has_value()) {
      continue;
    }
    if (picked == EDITOR_PLACEMENT_NONE || *hit > nearest) {
      picked = static_cast<int>(i);
      nearest = *hit;
    }
  }
  return picked;
}

}  // namespace eng::editor
