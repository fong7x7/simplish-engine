#pragma once

// Design Summary -- PS5 Save Data
// Technical Approach:
// docs/technical-approaches/engine/platform-ps5/system-services.md
//
// Behaviours:
//   - Save world data via SceLibSaveData with one slot per world seed
//   - Async save/load on background thread; never blocks render thread
//   - Display PS5 system save dialog on error (corrupted, full storage)
//   - Save data size declared at installation via param.sfo
//
// Edge Cases:
//   - Save data corrupted: display system dialog, offer delete + recreate
//   - Storage full: display system dialog prompting user to free space
//   - Save interrupted by suspend: resume on wake, verify integrity
//
// Invariants:
//   - Save/load never blocks the main thread
//   - PS5 SDK headers never in this public header
//
// Integration Points:
//   - World storage: wraps engine serialisation output
//   - EventBus: save complete/error events

#include "ps5-types.h"

#include <cstddef>
#include <cstdint>
#include <engine/core/event-bus.h>
#include <engine/core/expected-polyfill.h>
#include <span>
#include <string_view>
#include <vector>

namespace eng {

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

/// Configuration for PS5 save data subsystem.
struct Ps5SaveConfig {
  /// Non-owning pointer to the engine event bus.
  EventBus* event_bus = nullptr;
  /// Maximum save data size from param.sfo; 0 = use SDK default.
  uint32_t max_save_size_bytes = 0;
};

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Initialise PS5 save data subsystem.
/// Main thread only.
std::expected<void, Ps5Error> initPs5SaveData(const Ps5SaveConfig& config);

/// Shut down PS5 save data subsystem. Waits for any in-flight save
/// to complete before returning.
/// Main thread only.
void shutdownPs5SaveData();

/// Save world data asynchronously. The data is copied internally.
/// Completion is signalled via EventBus. Returns error if a save
/// is already in progress for this world.
/// Main thread only.
std::expected<void, Ps5Error> saveWorldAsync(std::string_view world_seed,
                                             std::span<const std::byte> data);

/// Load world data asynchronously. Completion is signalled via EventBus
/// with the loaded byte buffer. Returns error if the save slot does
/// not exist.
/// Main thread only.
std::expected<void, Ps5Error> loadWorldAsync(std::string_view world_seed);

/// Returns true if a save operation is currently in progress.
bool isSaveInProgress();

}  // namespace eng
