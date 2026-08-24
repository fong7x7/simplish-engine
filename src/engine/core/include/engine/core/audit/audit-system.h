#pragma once

#include "audit-config.h"
#include "audit-errors.h"
#include "audit-merge-context.h"
#include "audit-persistence.h"
#include "audit-query.h"
#include "audit-ring-buffer.h"
#include "audit-schema.h"
#include "audit-types.h"

#include <cstdint>
#include <optional>
#include <unordered_set>

namespace eng {

// ============================================================================
// DESIGN SUMMARY -- Audit System
// Technical Approach: docs/technical-approaches/engine/audit.md
//
// Behaviours:
//   - Initialize the full audit system: ring buffers, merge thread, schema
//     registry, error registry, persistence, query context
//   - Shut down cleanly: drain buffers, flush to disk, join threads, free
//   memory
//   - Enable/disable audit at runtime (master toggle)
//   - Enable/disable individual event categories at runtime
//   - Register per-thread ring buffers during thread init
//   - Provide access to query context for event filtering and statistics
//
// Edge Cases:
//   - Init with disabled config: create minimal context, skip ring buffer alloc
//   - Double init: ENGINE_ASSERT
//   - Shutdown before init: no-op
//   - setCategoryEnabled for unknown category: silently ignored
//
// Invariants:
//   - AuditSystemContext is the single owner of all audit subsystem state
//   - Init must be called before any AUDIT_EVENT macro is used
//   - Shutdown joins the merge thread and flushes all pending events
//   - All dependencies injectable via config and function pointers
//
// Integration Points:
//   - Engine init/shutdown: calls initAuditSystem / shutdownAuditSystem
//   - Thread pool: each worker thread calls registerAuditThread on start
//   - Plugin API: AuditDomain delegates to audit system functions
//   - Event Log Viewer: accesses AuditQueryContext via getAuditQueryContext
// ============================================================================

// --- System context (owns all audit subsystem state) ---

struct AuditSystemContext {
  /// Audit configuration snapshot used during initialization.
  AuditConfig config;
  /// Registry of event payload schemas for serialization and display.
  AuditSchemaRegistry schema_registry;
  /// Registry of error code definitions for resolution and display.
  AuditErrorRegistry error_registry;
  /// Merge context owning per-thread buffers and the merge thread.
  AuditMergeContext merge_ctx;
  /// Persistence context for flushing events to .vxaudit files.
  AuditPersistenceContext persistence_ctx;
  /// Query context for filtering, statistics, bookmarks, and subscriptions.
  AuditQueryContext query_ctx;
  /// Set of disabled event categories (main-thread-only).
  std::unordered_set<uint16_t> disabled_categories;
  /// Injectable clock source returning monotonic nanoseconds.
  TimestampFn timestamp_fn{};
  /// Non-owning pointer to the engine's frame counter.
  uint64_t* frame_counter{};
};

/// Toggle for enabling/disabling audit features (replaces bare bool).
enum class AuditToggle { ENABLED, DISABLED };

// --- Public API ---

/// Initialize the audit system. Loads schemas and error codes, allocates
/// ring buffers, starts the merge thread, opens the initial .vxaudit file.
/// @param config Audit configuration (from config/audit.toml).
/// @param timestamp_fn Injectable clock source (monotonic nanoseconds).
/// @param frame_counter Pointer to the engine's frame counter.
/// @param data_path Base path for data/audit/ directory.
/// Returns nullopt if critical initialization fails (buffer allocation).
std::optional<AuditSystemContext> initAuditSystem(const AuditConfig& config,
                                                  TimestampFn timestamp_fn,
                                                  uint64_t* frame_counter,
                                                  std::string_view data_path);

/// Shut down the audit system. Drains all buffers, flushes to disk,
/// joins the merge thread, frees all memory. Safe to call on a
/// partially initialized context.
void shutdownAuditSystem(AuditSystemContext& ctx);

/// Enable or disable audit event emission at runtime (master toggle).
/// When disabled, all AUDIT_EVENT macros early-return.
void setAuditEnabled(AuditSystemContext& ctx, AuditToggle toggle);

/// Enable or disable a specific event category at runtime.
void setCategoryEnabled(AuditSystemContext& ctx, uint16_t category,
                        AuditToggle toggle);

/// Register a per-thread ring buffer for the calling thread.
/// Must be called by each worker thread before emitting audit events.
/// Allocates a per-thread buffer and registers it with the merge context.
/// Returns false if allocation fails.
bool registerAuditThread(AuditSystemContext& ctx);

/// Deregister the calling thread's ring buffer.
/// Drains remaining events before deregistration.
void deregisterAuditThread(AuditSystemContext& ctx);

/// Get a read-only reference to the query context for event filtering,
/// statistics, bookmarks, and subscriptions.
const AuditQueryContext& getAuditQueryContext(const AuditSystemContext& ctx);

/// Get a mutable reference to the query context (for bookmarks and subs).
AuditQueryContext& getAuditQueryContextMut(AuditSystemContext& ctx);

/// Force an immediate flush of the ring buffer to disk.
void flushAuditToDisk(AuditSystemContext& ctx);

}  // namespace eng
