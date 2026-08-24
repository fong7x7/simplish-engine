#pragma once

// Design Summary -- Epic Commerce (Entitlements & DLC)
// Technical Approach:
//   docs/technical-approaches/engine/platform-epic/commerce.md
//
// Behaviours:
//   - Query owned entitlements for authenticated user (async)
//   - Check ownership of specific entitlement by name (cached, sync)
//   - Redeem one-time entitlements (async)
//   - Cache invalidated on session start and purchase callback
//
// Edge Cases:
//   - EOS not available: ownership checks return false
//   - Not authenticated: query fails; checks return false
//   - Entitlement not found: ownsEntitlement returns false
//   - Redeem already-consumed: callback receives error
//
// Invariants:
//   - Entitlement checks never block gameplay
//   - All operations main-thread-only
//   - DLC gating enforced by game code, not engine
//
// Integration Points:
//   - Game modding: DLC-exclusive mod filtering
//   - Game UI: lock icons, store links on gated content
//   - EventBus: on_epic_entitlement_changed emitted after purchase

#include "epic-types.h"

#include <cstdint>
#include <engine/core/expected-polyfill.h>
#include <functional>
#include <string_view>

namespace eng {

// Forward declarations
struct EpicContext;

// ---------------------------------------------------------------------------
// Callback types
// ---------------------------------------------------------------------------

using EpicEntitlementQueryCallback =
    std::function<void(std::expected<uint32_t, EpicError> count)>;

using EpicRedeemCallback = std::function<void(EpicError error)>;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Query all owned entitlements for the authenticated user. Async;
/// callback receives the entitlement count or error. Populates
/// local cache on success.
/// Main thread only.
void epicQueryEntitlements(const EpicContext& ctx,
                           const EpicEntitlementQueryCallback& callback);

/// Check if the user owns a specific entitlement by name.
/// Synchronous; reads from local cache. Returns false if cache
/// is empty or EOS is unavailable.
/// Main thread only.
bool epicOwnsEntitlement(const EpicContext& ctx,
                         std::string_view entitlement_name);

/// Redeem a one-time entitlement (e.g. bonus items). Async;
/// callback receives error status. Main thread only.
void epicRedeemEntitlement(const EpicContext& ctx,
                           std::string_view entitlement_name,
                           const EpicRedeemCallback& callback);

}  // namespace eng
