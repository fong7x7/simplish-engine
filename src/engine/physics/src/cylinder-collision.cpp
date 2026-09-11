#include <algorithm>
#include <cmath>
#include <engine/physics/cylinder-collision.h>

namespace eng::physics {

namespace {

  /// Whether the cylinder's height and the box's overlap at all.
  bool heightsOverlap(const CollisionCylinder& cylinder,
                      const CollisionBox& box) {
    return cylinder.bottom < box.max.z &&
           cylinder.bottom + cylinder.height > box.min.z;
  }

  /// The point of the box's footprint nearest @p point.
  Vec2 closestOnFootprint(const CollisionBox& box, Vec2 point) {
    return {std::clamp(point.x, box.min.x, box.max.x),
            std::clamp(point.y, box.min.y, box.max.y)};
  }

  /// The overlap distance the skin allows, squared.
  float allowedSquared(float radius) {
    const float allowed = std::max(0.0F, radius - COLLISION_SKIN);
    return allowed * allowed;
  }

  /// @p center, whose circle overlaps the footprint's edge from outside,
  /// moved straight away from @p closest until it just touches.
  Vec2 pushFromEdge(Vec2 center, Vec2 closest, float radius) {
    const Vec2 away{center.x - closest.x, center.y - closest.y};
    const float distance = std::sqrt(away.x * away.x + away.y * away.y);
    return {closest.x + away.x * (radius / distance),
            closest.y + away.y * (radius / distance)};
  }

  /// @p center, inside the footprint, moved out through its nearest face
  /// to just touch it. Ties go to the face tested first — -X, +X, -Y, +Y —
  /// so the answer never depends on anything but the numbers.
  Vec2 pushFromInside(Vec2 center, const CollisionBox& box, float radius) {
    const float to_min_x = center.x - box.min.x;
    const float to_max_x = box.max.x - center.x;
    const float to_min_y = center.y - box.min.y;
    const float to_max_y = box.max.y - center.y;
    const float nearest = std::min({to_min_x, to_max_x, to_min_y, to_max_y});
    if (nearest == to_min_x) {
      return {box.min.x - radius, center.y};
    }
    if (nearest == to_max_x) {
      return {box.max.x + radius, center.y};
    }
    return nearest == to_min_y ? Vec2{center.x, box.min.y - radius}
                               : Vec2{center.x, box.max.y + radius};
  }

  /// @p cylinder moved out of @p box, or where it was when it is not in it.
  Vec2 pushOutOf(const CollisionCylinder& cylinder, const CollisionBox& box) {
    const Vec2 closest = closestOnFootprint(box, cylinder.center);
    const bool inside =
        closest.x == cylinder.center.x && closest.y == cylinder.center.y;
    return inside ? pushFromInside(cylinder.center, box, cylinder.radius)
                  : pushFromEdge(cylinder.center, closest, cylinder.radius);
  }

  /// One pass over @p boxes, moving @p cylinder out of each it overlaps.
  /// Returns whether anything moved it.
  bool resolvePass(CollisionCylinder& cylinder,
                   std::span<const CollisionBox> boxes) {
    bool moved = false;
    for (const CollisionBox& box : boxes) {
      if (cylinderOverlapsBox(cylinder, box)) {
        cylinder.center = pushOutOf(cylinder, box);
        moved = true;
      }
    }
    return moved;
  }

  /// `resolvePass` over only the boxes at @p candidates.
  bool resolvePassOver(CollisionCylinder& cylinder,
                       std::span<const CollisionBox> boxes,
                       std::span<const uint32_t> candidates) {
    bool moved = false;
    for (const uint32_t index : candidates) {
      if (cylinderOverlapsBox(cylinder, boxes[index])) {
        cylinder.center = pushOutOf(cylinder, boxes[index]);
        moved = true;
      }
    }
    return moved;
  }

}  // namespace

bool cylinderOverlapsBox(const CollisionCylinder& cylinder,
                         const CollisionBox& box) {
  if (!heightsOverlap(cylinder, box)) {
    return false;
  }
  const Vec2 closest = closestOnFootprint(box, cylinder.center);
  const float dx = cylinder.center.x - closest.x;
  const float dy = cylinder.center.y - closest.y;
  return dx * dx + dy * dy < allowedSquared(cylinder.radius);
}

Vec2 resolveCylinderAgainstBoxes(const CollisionCylinder& cylinder,
                                 std::span<const CollisionBox> boxes) {
  CollisionCylinder moving = cylinder;
  for (int pass = 0; pass < COLLISION_RESOLVE_PASSES; ++pass) {
    if (!resolvePass(moving, boxes)) {
      break;
    }
  }
  return moving.center;
}

Vec2 resolveCylinderAgainstBoxes(const CollisionCylinder& cylinder,
                                 std::span<const CollisionBox> boxes,
                                 std::span<const uint32_t> candidates) {
  CollisionCylinder moving = cylinder;
  for (int pass = 0; pass < COLLISION_RESOLVE_PASSES; ++pass) {
    if (!resolvePassOver(moving, boxes, candidates)) {
      break;
    }
  }
  return moving.center;
}

}  // namespace eng::physics
