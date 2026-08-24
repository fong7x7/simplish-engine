#pragma once

#include "audit-actor-stats.h"
#include "audit-bookmark.h"
#include "audit-error-summary.h"
#include "audit-errors.h"
#include "audit-event-record.h"
#include "audit-event.h"
#include "audit-filter.h"
#include "audit-merge-context.h"
#include "audit-schema.h"
#include "audit-stats.h"
#include "audit-types.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace eng {

// ============================================================================
// DESIGN SUMMARY -- Audit Query Engine
// Technical Approach: docs/technical-approaches/engine/audit/query.md
//
// Behaviours:
//   - Query events from merge buffer using multi-field filter
//   - Compute aggregate statistics (counts, rates, buffer/disk usage)
//   - Return top-N actors by event count
//   - Return error summary grouped by error code
//   - Manage named bookmarks on the event timeline
//   - Subscribe/unsubscribe to real-time event callbacks
//
// Edge Cases:
//   - Empty merge buffer: return empty results / zero stats
//   - Filter matches nothing: return empty vector (not an error)
//   - Time range start > end: return empty result
//   - Regex compilation failure: return expected with error
//   - Duplicate bookmark name: overwrite previous
//   - Invalid unsubscribe handle: return false, no side effect
//
// Invariants:
//   - Query results are a snapshot; do not update as new events arrive
//   - Statistics computed from merge buffer state at query time
//   - Bookmarks are session-scoped (not persisted)
//   - Callbacks invoked on merge thread
//   - Query does not modify the merge buffer
//
// Integration Points:
//   - audit-ring-buffer.h: reads merge buffer for event data
//   - audit-schema.h: decodes payloads for text search
//   - audit-errors.h: resolves error codes for error summary
//   - Event Log Viewer (editor layer): primary consumer
//   - Plugin API: audit_get_events, audit_get_stats, audit_bookmark
// ============================================================================

// --- Query error ---

enum class AuditQueryError : uint8_t {
  INVALID_REGEX,
};

// --- Event subscription callback ---

using AuditEventCallback = void (*)(const AuditEventRecord& event);

// --- Query context ---

struct AuditQueryContext {
  /// Non-owning pointer to the merge context (event data source).
  const AuditMergeContext* merge_ctx{};
  /// Non-owning pointer to the schema registry (for payload decoding).
  const AuditSchemaRegistry* schema_registry{};
  /// Non-owning pointer to the error registry (for error code resolution).
  const AuditErrorRegistry* error_registry{};
  /// Session-scoped named bookmarks on the event timeline.
  std::vector<AuditBookmark> bookmarks;

  struct Subscription {
    /// Unique subscription handle for unsubscription.
    uint64_t handle{};
    /// Callback invoked on the merge thread for each new event.
    AuditEventCallback callback{};
  };
  /// Active real-time event subscriptions.
  std::vector<Subscription> subscriptions;
  /// Counter for generating unique subscription handles.
  uint64_t next_sub_handle = 1;
};

// --- Public API ---

/// Create a query context with non-owning references to shared state.
AuditQueryContext createQueryContext(const AuditMergeContext& merge,
                                     const AuditSchemaRegistry& schema,
                                     const AuditErrorRegistry& errors);

/// Query result: either a vector of matching records or a query error.
using AuditQueryResult =
    std::variant<std::vector<AuditEventRecord>, AuditQueryError>;

/// Query events matching the filter. Returns matching records up to limit.
/// Returns AuditQueryError if text_pattern contains an invalid regex.
AuditQueryResult queryEvents(const AuditQueryContext& ctx,
                             const AuditFilter& filter);

/// Compute aggregate audit statistics at the current moment.
AuditStats getAuditStats(const AuditQueryContext& ctx);

/// Return the top-N actors by event count within a time window.
std::vector<AuditActorStats> getTopActors(const AuditQueryContext& ctx,
                                          uint32_t top_n, uint64_t start_ns,
                                          uint64_t end_ns);

/// Return error occurrence summary grouped by error code.
std::vector<AuditErrorSummary> getErrorSummary(const AuditQueryContext& ctx);

/// Place a named bookmark at a timestamp. Overwrites if name already exists.
void addBookmark(AuditQueryContext& ctx, std::string_view name,
                 uint64_t timestamp_ns);

/// List all bookmarks.
std::span<const AuditBookmark> getBookmarks(const AuditQueryContext& ctx);

/// Remove a bookmark by name. Returns false if not found.
bool removeBookmark(AuditQueryContext& ctx, std::string_view name);

/// Subscribe to real-time audit events. Returns handle for unsubscription.
/// Callback is invoked on the merge thread for each new event.
uint64_t subscribeEvents(AuditQueryContext& ctx, AuditEventCallback callback);

/// Unsubscribe from real-time audit events. Returns false if handle not found.
bool unsubscribeEvents(AuditQueryContext& ctx, uint64_t handle);

}  // namespace eng
