#pragma once

#include "audit-error-entry.h"
#include "audit-event-header.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace eng {

// ============================================================================
// DESIGN SUMMARY -- Audit Error Registry
// Technical Approach: docs/technical-approaches/engine/audit/schema-errors.md
//
// Behaviours:
//   - Load error code definitions from JSON into hash map keyed by uint32 code
//   - Look up error by numeric code in O(1)
//   - Look up error by symbolic name via secondary index
//   - Register mod-defined error codes at runtime (codes >= 100000)
//
// Edge Cases:
//   - Duplicate error code: reject, first wins
//   - Mod code below 100000: reject with warning
//   - Malformed JSON: return nullopt, engine continues with empty registry
//   - Lookup for unregistered code: return nullptr
//   - Lookup for unregistered symbol: return nullptr
//
// Invariants:
//   - Error registry is append-only after initial load
//   - Lookup by code is O(1) via hash map
//   - Lookup by symbol is O(1) via secondary index
//   - Error code ranges enforced: base engine 1000-69999, mods 100000+
//
// Integration Points:
//   - audit-emit.h: AUDIT_EVENT_ERROR writes error_code into payload
//   - audit-query.h: error summary and error filtering
//   - Event Log Viewer: resolves codes to display text
// ============================================================================

struct AuditErrorRegistry {
  /// Primary index: error code to error entry.
  std::unordered_map<uint32_t, AuditErrorEntry> by_code;
  /// Secondary index: symbolic name to error code for O(1) symbol lookup.
  std::unordered_map<std::string, uint32_t> symbol_to_code;
};

// --- Public API ---

/// Load error code definitions from the given base directory.
/// Reads data/audit/error_codes.json (and mod error files).
/// Returns nullopt if the base JSON file is missing or malformed.
std::optional<AuditErrorRegistry> loadErrorRegistry(std::string_view base_path);

/// Look up an error definition by numeric code. O(1).
/// Returns nullptr if no error is registered for this code.
const AuditErrorEntry* lookupError(const AuditErrorRegistry& registry,
                                   uint32_t code);

/// Look up an error definition by symbolic name. O(1).
/// Returns nullptr if no error is registered with this symbol.
const AuditErrorEntry* lookupErrorBySymbol(const AuditErrorRegistry& registry,
                                           std::string_view symbol);

/// Register a mod-defined error code at runtime.
/// Returns false if code < AUDIT_MOD_ERROR_CODE_MIN or if the code/symbol
/// already exists.
bool registerModError(AuditErrorRegistry& registry,
                      const AuditErrorEntry& entry);

}  // namespace eng
