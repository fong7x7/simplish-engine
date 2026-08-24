#pragma once

// Design Summary -- Steam Workshop
// Technical Approach:
// docs/technical-approaches/engine/platform-steam/workshop.md
//
// Behaviours:
//   - Subscribe to a Workshop item by published file ID
//   - Query install path and status of a subscribed item
//   - Create a new Workshop item for publishing
//   - Submit an item update with content, metadata, and change note
//   - Query all subscribed items and their install status
//
// Edge Cases:
//   - Steam not available: all operations return NOT_AVAILABLE
//   - Item not downloaded: getItemInstallInfo returns
//   WORKSHOP_ITEM_NOT_INSTALLED
//   - Item ID does not exist: returns WORKSHOP_ITEM_NOT_FOUND
//   - Publish fails: returns WORKSHOP_PUBLISH_FAILED
//   - Empty content folder: returns INVALID_ARGUMENT
//   - Multiple subscribe for same item: idempotent
//
// Invariants:
//   - Workshop items are .vxmod archives loaded by mod manager
//   - Workshop versions take precedence over local mods with same mod_id
//   - Download events emitted via EventBus; mod manager subscribes
//   - Async operations (create, submit) deliver results via EventBus events
//   - Main thread only
//
// Integration Points:
//   - Mod Manager: reads install paths; subscribes to download events
//   - Editor: publish flow uses createItem() + submitItemUpdate()
//   - EventBus: SteamWorkshopItemInstalled, SteamWorkshopItemCreated events

#include "steam-types.h"
#include "steam-workshop-item-info.h"
#include "steam-workshop-update-params.h"

#include <cstdint>
#include <engine/core/expected-polyfill.h>
#include <string_view>
#include <vector>

namespace eng {

// Forward declaration
struct SteamContext;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Subscribe to a Workshop item. Triggers download if not already
/// installed. Download completion is signalled via a
/// SteamWorkshopItemInstalled EventBus event.
/// Main thread only.
std::expected<bool, SteamError> subscribeItem(const SteamContext& ctx,
                                              uint64_t item_id);

/// Get install information for a subscribed Workshop item. Returns
/// WORKSHOP_ITEM_NOT_INSTALLED if the item is subscribed but not yet
/// downloaded.
/// Main thread only.
std::expected<SteamWorkshopItemInfo, SteamError>
getItemInstallInfo(const SteamContext& ctx, uint64_t item_id);

/// Create a new Workshop item for publishing. The item ID is delivered
/// asynchronously via a SteamWorkshopItemCreated EventBus event.
/// Returns an error if creation could not be initiated.
/// Main thread only.
std::expected<uint64_t, SteamError> createItem(const SteamContext& ctx);

/// Submit an item update with content, metadata, and change note.
/// Returns true if the update was submitted. Progress and completion
/// are delivered via SteamWorkshopUpdateProgress and
/// SteamWorkshopUpdateComplete EventBus events.
/// Main thread only.
std::expected<bool, SteamError>
submitItemUpdate(const SteamContext& ctx, uint64_t item_id,
                 const SteamWorkshopUpdateParams& params);

/// Get information about all subscribed Workshop items.
/// Items that are not yet downloaded will have is_installed == false.
/// Main thread only.
std::vector<SteamWorkshopItemInfo> getSubscribedItems(const SteamContext& ctx);

}  // namespace eng
