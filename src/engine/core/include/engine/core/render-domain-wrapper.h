#pragma once

#include "plugin-api.h"

namespace eng {

/// Type-safe C++ wrapper around the C ABI render domain function table.
/// Thread Safety: main-thread-only (same as PluginAPI contract).
class RenderDomainWrapper {
public:
  explicit RenderDomainWrapper(const PluginAPI::RenderDomain& api)
    : api_(&api) {}

  Vec3 getCameraPos() const;

  uint32_t getVisibleEntities(EntityID* out_entities,
                              uint32_t max_count) const {
    return api_->get_visible_entities(out_entities, max_count);
  }

private:
  /// Pointer to the C ABI render domain function table.
  const PluginAPI::RenderDomain* api_;
};

}  // namespace eng
