#pragma once

// Design Summary -- Xbox Series X Save Data (Connected Storage)
// Technical Approach:
// docs/technical-approaches/engine/platform-xbox-series-x/save-data.md
//
// Behaviours:
//   - Initialise Connected Storage provider for signed-in user
//   - Save world data to container named by world seed (async)
//   - Load world data from container (async)
//   - Delete a save container
//   - Handle cloud sync conflicts with user-facing UI
//   - Display error UI for corrupted saves and storage-full
//   - Flush pending saves on suspend
//
// Edge Cases:
//   - User not signed in: return error, save/load disabled
//   - Cloud sync conflict: present resolution UI, user chooses
//   - Corrupted save: return SAVE_CORRUPTED, offer delete
//   - Storage full: return STORAGE_FULL, prompt user
//   - Save during suspend: must complete before PLM ack
//   - User signs out during save: pending writes may fail
//   - Load nonexistent container: return SAVE_NOT_FOUND
//
// Invariants:
//   - Save operations never block the render thread
//   - One container per world (named by world seed string)
//   - Connected Storage auto-syncs to cloud when online
//   - Provider recreated on user change
//
// Integration Points:
//   - Engine save/load: Connected Storage is one backend
//   - Lifecycle: flush saves on suspend
//   - User management: recreate provider on user change

#include "xbox-save-config.h"
#include "xbox-save-context.h"
#include "xbox-types.h"

#include <cstddef>
#include <engine/core/expected-polyfill.h>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

namespace eng {

// ---------------------------------------------------------------------------
// Callback types
// ---------------------------------------------------------------------------

/// Callback for save completion.
using XboxSaveCallback = std::function<void(std::expected<bool, XboxError>)>;

/// Callback for load completion. Receives loaded data or error.
using XboxLoadCallback =
    std::function<void(std::expected<std::vector<std::byte>, XboxError>)>;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Initialise Connected Storage provider for the signed-in user.
/// Must be called after user sign-in. Provider is async-created;
/// returns context immediately with provider_valid=false until ready.
/// Main thread only.
std::expected<XboxSaveContext, XboxError>
initXboxSaveProvider(const XboxSaveConfig& config);

/// Shut down the Connected Storage provider. Releases resources.
/// Idempotent.
/// Main thread only.
void shutdownXboxSaveProvider(XboxSaveContext& ctx);

/// Save data to a Connected Storage container. The container_name
/// is typically the world seed string. Operation is asynchronous;
/// result delivered via callback. Never blocks the render thread.
/// Main thread only.
void saveXboxData(XboxSaveContext& ctx, std::string_view container_name,
                  std::span<const std::byte> data, XboxSaveCallback callback);

/// Load data from a Connected Storage container. Operation is
/// asynchronous; result delivered via callback.
/// Main thread only.
void loadXboxData(XboxSaveContext& ctx, std::string_view container_name,
                  XboxLoadCallback callback);

/// Delete a save container. Returns synchronously.
/// Main thread only.
std::expected<bool, XboxError>
deleteXboxSaveContainer(XboxSaveContext& ctx, std::string_view container_name);

/// Flush all pending save operations. Blocks until complete.
/// Called during suspend to ensure data is persisted before
/// PLM acknowledgement.
/// Main thread only.
void flushXboxPendingSaves(XboxSaveContext& ctx);

}  // namespace eng
