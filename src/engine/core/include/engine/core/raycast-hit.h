#pragma once

#include "engine-config.h"

#include <engine/math/math.h>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// RaycastHit: C ABI-safe result struct for ray intersection queries.
//
// Contains the hit position, surface normal, distance from origin, and the
// entity ID of the hit object. Used by both voxel and physics raycasts.
//
// Thread Safety:
// - POD type; no thread-safety concerns.
// ============================================================================

struct RaycastHit {
  /// World-space position of the hit point.
  Vec3 position{};
  /// Surface normal at the hit point.
  Vec3 normal{};
  /// Distance from ray origin to the hit point.
  float distance{};
  /// Entity ID of the hit object (0 if terrain/voxel).
  EntityID entity_id{};
};

}  // namespace eng
