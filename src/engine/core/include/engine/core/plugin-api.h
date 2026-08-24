#pragma once

#include "engine-config.h"
#include "raycast-hit.h"

#include <cstddef>
#include <cstdint>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// PluginAPI: C ABI contract between engine and C++ plugins (via dlopen).
//
// Responsibilities:
// - Engine domains: voxel, physics, world, render, audio, input query/mutate
// - Game domains: entity, goal, effect APIs (game layer populates)
// - Event subscription: plugins subscribe to named events
// - Custom type registration: conditions and actions
// - Async plugin support: task-based (submit/consume) and tick-loop (Hz)
// - Logging: plugin-safe logging through engine logger
//
// C ABI Boundary Rules:
// - All types are C-compatible: const char*, function pointers, void* + size
// - No std::function, std::any, std::string_view, std::vector across boundary
// - POD structs only (Vec3, RaycastHit)
// - Nullable domain pointers for headless mode (render, audio, input)
//
// Plugin Contract:
// - PluginAPI* valid from simplishPluginInit() to simplish_plugin_shutdown()
// - Do NOT cache PluginAPI*
// - Handlers complete in < 1 ms typical
// - No blocking I/O in handlers (use asyncSubmit or registerAsyncTick)
// - No recursive event emission
// - Async tick threads MUST NOT call PluginAPI domains directly
//
// API Lifetime:
// - Game layer owns PluginAPI*; do not free or modify
// - Valid only during init/shutdown and event handler invocation
// - Async threads: only via asyncSubmit/asyncConsumeResults
// ============================================================================

// =====================================================================
// Plugin API Contract (C ABI safe — passed to plugins via dlopen)
// =====================================================================
struct PluginAPI {
  /// Voxel query/mutate domain (always available).
  // ===== Voxel Domain =====
  struct VoxelDomain {
    /// Get voxel type at the given world coordinate.
    uint16_t (*get)(WorldVoxelCoord x, WorldVoxelCoord y, WorldVoxelCoord z);
    /// Set voxel type at the given world coordinate; returns true on success.
    bool (*set)(WorldVoxelCoord x, WorldVoxelCoord y, WorldVoxelCoord z,
                uint16_t type_id);
    /// Cast a ray and report the first voxel hit; returns true if hit found.
    bool (*raycast)(const Vec3* origin, const Vec3* direction, float max_dist,
                    RaycastHit* out_hit);
    /// Get voxel health at the given world coordinate (0.0 to 1.0).
    float (*get_health)(WorldVoxelCoord x, WorldVoxelCoord y,
                        WorldVoxelCoord z);
  } voxels;  ///< Voxel query/mutate operations.

  /// Physics query/mutate domain (always available).
  // ===== Physics Domain =====
  struct PhysicsDomain {
    /// Get physics bodies within radius of a position; returns count written.
    uint32_t (*get_bodies_at)(const Vec3* pos, float radius,
                              EntityID* out_bodies, uint32_t max_count);
    /// Cast a ray through the physics world; returns true if hit found.
    bool (*raycast)(const Vec3* origin, const Vec3* direction, float max_dist,
                    RaycastHit* out_hit);
    /// Apply a force vector to a physics body.
    void (*apply_force)(EntityID body, const Vec3* force);
  } physics;  ///< Physics query/force operations.

  /// World state domain (always available).
  // ===== World Domain =====
  struct WorldDomain {
    /// Get current world time in milliseconds.
    float (*get_time_ms)();
    /// Check whether a chunk at the given coordinates is currently loaded.
    bool (*is_chunk_loaded)(int64_t cx, int64_t cy, int64_t cz);
    /// Get an integer world flag by key name.
    int32_t (*get_flag)(const char* key);
    /// Set a boolean world flag by key name; returns true on success.
    /// NOLINTNEXTLINE(simplish-no-bool-params) — C ABI boundary; enum class not
    /// portable across dlopen
    bool (*set_flag)(const char* key, bool value);
    /// Get the deterministic world seed.
    uint64_t (*world_seed)();
  } world;  ///< World time/flag/chunk queries.

  /// Render domain — nullptr in headless mode.
  // ===== Render Domain (nullable — nullptr in HEADLESS) =====
  struct RenderDomain {
    /// Write the current camera world position into out_pos.
    void (*get_camera_pos)(Vec3* out_pos);
    /// Get IDs of currently visible entities; returns count written.
    uint32_t (*get_visible_entities)(EntityID* out_entities,
                                     uint32_t max_count);
  }* render;  ///< Camera and visibility queries (nullptr in headless).

  /// Audio domain — nullptr in headless mode.
  // ===== Audio Domain (nullable — nullptr in HEADLESS) =====
  struct AudioDomain {
    /// Play a sound file at a 3D world position.
    void (*play_sound)(const char* path, const Vec3* pos);
  }* audio;  ///< 3D sound playback (nullptr in headless).

  /// Input domain — nullptr in headless mode.
  // ===== Input Domain (nullable — nullptr in HEADLESS) =====
  struct InputDomain {
    /// Check whether a named input action is currently active.
    bool (*is_action_active)(const char* action_name);
    /// Get the current value of a named input axis (-1.0 to 1.0).
    float (*get_axis_value)(const char* axis_name);
  }* input;  ///< Action/axis queries (nullptr in headless).

  /// Entity domain populated by the game layer.
  // ===== Entity Domain (provided by game layer) =====
  struct EntityDomain {
    /// Spawn an entity of the given type at a position; returns its ID.
    EntityID (*spawn)(const char* type, const Vec3* pos);
    /// Despawn an entity by ID; returns true if found and removed.
    bool (*despawn)(EntityID id);
    /// Get an entity's world position; returns true if entity exists.
    bool (*get_position)(EntityID id, Vec3* out_pos);
    /// Get a raw pointer to a named component on an entity (nullptr if absent).
    void* (*get_component)(EntityID id, const char* component_type);
  } entity;  ///< Entity spawn/despawn/component queries.

  // ===== Event Subscription =====
  /// Subscribe to a named event; returns a handle for later unsubscription.
  SubscriptionHandle (*subscribe_event)(const char* event_name,
                                        void (*handler)(const char* event_name,
                                                        const void* payload,
                                                        size_t payload_size));
  /// Unsubscribe from an event by handle.
  void (*unsubscribe_event)(SubscriptionHandle handle);

  // ===== Condition & Action Registration =====
  /// Register a custom condition evaluator by type name.
  void (*register_condition)(const char* type_name,
                             bool (*evaluator)(const void* params,
                                               size_t params_size,
                                               const void* eval_ctx,
                                               size_t eval_ctx_size));
  /// Register a custom action executor by type name.
  void (*register_action)(const char* type_name,
                          bool (*executor)(const void* params,
                                           size_t params_size,
                                           const void* action_ctx,
                                           size_t action_ctx_size));

  // ===== Async Plugin Support (Task-Based) =====
  /// Submit an async task for background execution; returns task ID.
  TaskID (*async_submit)(const char* plugin_id,
                         void (*work_fn)(void* user_data, void* result_buf,
                                         size_t result_buf_size),
                         void* user_data);
  /// Consume completed async task results; returns count consumed.
  uint32_t (*async_consume_results)(const char* plugin_id, TaskID* out_task_ids,
                                    void** out_results, size_t* out_sizes,
                                    uint32_t max_results);
  /// Get the number of pending async tasks for this plugin.
  uint32_t (*async_get_pending_count)(const char* plugin_id);
  /// Cancel an in-flight async task; returns true if cancelled.
  bool (*async_cancel)(const char* plugin_id, TaskID task_id);

  // ===== Async Plugin Tick Loop =====
  /// Register a dedicated tick loop for a plugin at the given Hz rate.
  bool (*register_async_tick)(const char* plugin_id,
                              void (*tick_fn)(float delta_seconds,
                                              void* user_data),
                              void* user_data, float tick_rate_hz);
  /// Unregister the plugin's async tick loop.
  void (*unregister_async_tick)(const char* plugin_id);

  // ===== Logging =====
  /// Log an informational message from a plugin.
  void (*log_info)(const char* plugin_id, const char* message);
  /// Log a warning message from a plugin.
  void (*log_warn)(const char* plugin_id, const char* message);
  /// Log an error message from a plugin.
  void (*log_error)(const char* plugin_id, const char* message);
};

}  // namespace eng

// =====================================================================
// Plugin Entry Points (C ABI — exported by plugin shared library)
// =====================================================================
extern "C" {
bool simplishPluginInit(eng::PluginAPI* api);
void simplishPluginShutdown(void);
const char* simplishPluginName(void);
const char* simplishPluginVersion(void);
uint32_t simplishPluginApiVersion(void);
}
