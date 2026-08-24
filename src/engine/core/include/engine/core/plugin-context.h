#pragma once

#include "plugin-api.h"
#include "plugin-domains.h"

#include <memory>
#include <optional>
#include <string_view>

namespace eng {

/// Aggregates all typed domain wrappers for convenient plugin access.
/// Thread Safety: main-thread-only (same as PluginAPI contract).
class PluginContext {
public:
  PluginContext(PluginAPI& api, std::string_view plugin_id)
    : voxels_(api.voxels), physics_(api.physics), world_(api.world),
      entity_(api.entity), events_(api), async_(api, plugin_id),
      log_(api, plugin_id), raw_api_(&api) {
    initNullableDomains(api);
  }

  VoxelDomainWrapper& voxels() { return voxels_; }
  const VoxelDomainWrapper& voxels() const { return voxels_; }

  PhysicsDomainWrapper& physics() { return physics_; }
  const PhysicsDomainWrapper& physics() const { return physics_; }

  WorldDomainWrapper& world() { return world_; }
  const WorldDomainWrapper& world() const { return world_; }

  EntityDomainWrapper& entity() { return entity_; }
  const EntityDomainWrapper& entity() const { return entity_; }

  EventApi& events() { return events_; }

  AsyncApi& async() { return async_; }

  LogApi& log() { return log_; }
  const LogApi& log() const { return log_; }

  bool hasRender() const { return render_.has_value(); }

  RenderDomainWrapper& render() {
    return render_.value();  // NOLINT(bugprone-unchecked-optional-access)
  }
  const RenderDomainWrapper& render() const {
    return render_.value();  // NOLINT(bugprone-unchecked-optional-access)
  }

  bool hasAudio() const { return audio_.has_value(); }

  AudioDomainWrapper& audio() {
    return audio_.value();  // NOLINT(bugprone-unchecked-optional-access)
  }
  const AudioDomainWrapper& audio() const {
    return audio_.value();  // NOLINT(bugprone-unchecked-optional-access)
  }

  bool hasInput() const { return input_.has_value(); }

  InputDomainWrapper& input() {
    return input_.value();  // NOLINT(bugprone-unchecked-optional-access)
  }
  const InputDomainWrapper& input() const {
    return input_.value();  // NOLINT(bugprone-unchecked-optional-access)
  }

  /// Register a custom condition evaluator by type name.
  /// Called once during onInit; delegates to C ABI register_condition.
  void registerCondition(std::string_view type_name,
                         bool (*evaluator)(const void* params,
                                           size_t params_size,
                                           const void* eval_ctx,
                                           size_t eval_ctx_size)) {
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
    raw_api_->register_condition(type_name.data(), evaluator);
  }

  /// Register a custom action executor by type name.
  /// Called once during onInit; delegates to C ABI register_action.
  void registerAction(std::string_view type_name,
                      bool (*executor)(const void* params, size_t params_size,
                                       const void* action_ctx,
                                       size_t action_ctx_size)) {
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
    raw_api_->register_action(type_name.data(), executor);
  }

  PluginAPI& rawApi() { return *raw_api_; }
  const PluginAPI& rawApi() const { return *raw_api_; }

  PluginContext(const PluginContext&) = delete;
  PluginContext& operator=(const PluginContext&) = delete;
  PluginContext(PluginContext&&) = delete;
  PluginContext& operator=(PluginContext&&) = delete;

private:
  void initNullableDomains(PluginAPI& api);

  /// Type-safe wrapper around the voxel domain API.
  VoxelDomainWrapper voxels_;
  /// Type-safe wrapper around the physics domain API.
  PhysicsDomainWrapper physics_;
  /// Type-safe wrapper around the world domain API.
  WorldDomainWrapper world_;
  /// Type-safe wrapper around the entity domain API.
  EntityDomainWrapper entity_;
  /// Type-safe wrapper around the event subscription API.
  EventApi events_;
  /// Type-safe wrapper around the async task submission API.
  AsyncApi async_;
  /// Type-safe wrapper around the plugin logging API.
  LogApi log_;
  /// Optional render domain (nullptr in headless mode).
  std::optional<RenderDomainWrapper> render_;
  /// Optional audio domain (nullptr in headless mode).
  std::optional<AudioDomainWrapper> audio_;
  /// Optional input domain (nullptr in headless mode).
  std::optional<InputDomainWrapper> input_;
  /// Raw C ABI plugin API pointer for low-level access.
  PluginAPI* raw_api_;
};

}  // namespace eng
