#pragma once

#include "plugin-api.h"
#include "raycast-hit.h"

#include <optional>

namespace eng {

/// Type-safe C++ wrapper around the C ABI voxel domain function table.
/// Thread Safety: main-thread-only (same as PluginAPI contract).
class VoxelDomainWrapper {
public:
  explicit VoxelDomainWrapper(const PluginAPI::VoxelDomain& api) : api_(&api) {}

  uint16_t get(WorldVoxelCoord x, WorldVoxelCoord y, WorldVoxelCoord z) const {
    return api_->get(x, y, z);
  }

  /// Parameters for setting a voxel via the plugin API.
  struct SetParams {
    /// Voxel X coordinate.
    WorldVoxelCoord x;
    /// Voxel Y coordinate.
    WorldVoxelCoord y;
    /// Voxel Z coordinate.
    WorldVoxelCoord z;
    /// Voxel type identifier.
    uint16_t type_id;
  };

  bool set(const SetParams& p) { return api_->set(p.x, p.y, p.z, p.type_id); }

  std::optional<RaycastHit> raycast(const Vec3& origin, const Vec3& direction,
                                    float max_dist) const {
    RaycastHit hit{};
    if (api_->raycast(&origin, &direction, max_dist, &hit)) {
      return hit;
    }
    return std::nullopt;
  }

  float getHealth(WorldVoxelCoord x, WorldVoxelCoord y,
                  WorldVoxelCoord z) const {
    return api_->get_health(x, y, z);
  }

private:
  /// Pointer to the C ABI voxel domain function table.
  const PluginAPI::VoxelDomain* api_;
};

}  // namespace eng
