#pragma once

#include "plugin-api.h"

#include <optional>
#include <string_view>

namespace eng {

/// Type-safe C++ wrapper around the C ABI entity domain function table.
/// Thread Safety: main-thread-only (same as PluginAPI contract).
class EntityDomainWrapper {
public:
  explicit EntityDomainWrapper(const PluginAPI::EntityDomain& api)
    : api_(&api) {}

  EntityID spawn(std::string_view type, const Vec3& pos) {
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
    return api_->spawn(type.data(), &pos);
  }

  bool despawn(EntityID id) { return api_->despawn(id); }

  std::optional<Vec3> getPosition(EntityID id) const;

  void* getComponent(EntityID id, std::string_view component_type) {
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
    return api_->get_component(id, component_type.data());
  }

private:
  /// Pointer to the C ABI entity domain function table.
  const PluginAPI::EntityDomain* api_;
};

}  // namespace eng
