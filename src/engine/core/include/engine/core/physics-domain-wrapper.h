#pragma once

#include "plugin-api.h"
#include "raycast-hit.h"

#include <optional>

namespace eng {

/// Type-safe C++ wrapper around the C ABI physics domain function table.
/// Thread Safety: main-thread-only (same as PluginAPI contract).
class PhysicsDomainWrapper {
public:
  explicit PhysicsDomainWrapper(const PluginAPI::PhysicsDomain& api)
    : api_(&api) {}

  /// Parameters for querying bodies at a position.
  struct GetBodiesAtParams {
    /// Query centre position.
    const Vec3& pos;
    /// Search radius.
    float radius;
    /// Output buffer for body IDs.
    EntityID* out_bodies;
    /// Maximum number of results.
    uint32_t max_count;
  };

  uint32_t getBodiesAt(const GetBodiesAtParams& p) const {
    return api_->get_bodies_at(&p.pos, p.radius, p.out_bodies, p.max_count);
  }

  std::optional<RaycastHit> raycast(const Vec3& origin, const Vec3& direction,
                                    float max_dist) const {
    RaycastHit hit{};
    if (api_->raycast(&origin, &direction, max_dist, &hit)) {
      return hit;
    }
    return std::nullopt;
  }

  void applyForce(EntityID body, const Vec3& force) {
    api_->apply_force(body, &force);
  }

private:
  /// Pointer to the C ABI physics domain function table.
  const PluginAPI::PhysicsDomain* api_;
};

}  // namespace eng
