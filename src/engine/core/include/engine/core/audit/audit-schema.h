#pragma once

#include "audit-schema-entry.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace eng {

// ============================================================================
// DESIGN SUMMARY -- Audit Schema Registry
// Technical Approach: docs/technical-approaches/engine/audit/schema-errors.md
//
// Behaviours:
//   - Load event schema definitions from JSON and compile into hash table
//   - Look up schema by (category, event_type) in O(1)
//   - Register mod-defined schemas at runtime
//   - Compute schema version hash for .vxaudit file headers
//
// Edge Cases:
//   - Duplicate (category, event_type): reject, first wins
//   - Unknown field type in JSON: skip field with warning
//   - Malformed JSON: return nullopt, engine continues with empty registry
//   - Lookup for unregistered schema: return nullopt
//
// Invariants:
//   - Schema registry is append-only after initial load
//   - Lookup is O(1) via hash map
//   - Schema field order matches binary payload layout (positional)
//
// Integration Points:
//   - audit-emit.h: plugin emission serialises fields using registered schemas
//   - audit-query.h: payload summary generation for text search
//   - audit-persistence.h: schema version hash in .vxaudit headers
// ============================================================================

struct AuditSchemaRegistry {
  /// Hash map from packed (category << 16 | event_type) key to schema entry.
  std::unordered_map<uint32_t, AuditSchemaEntry> entries;
};

// --- Public API ---

/// Load and compile event schemas from the given base directory.
/// Reads data/audit/event_schemas.json (and mod schema files).
/// Returns nullopt if the base JSON file is missing or malformed.
std::optional<AuditSchemaRegistry>
loadSchemaRegistry(std::string_view base_path);

/// Look up a schema by (category, event_type). O(1).
/// Returns nullptr if no schema is registered for this pair.
const AuditSchemaEntry* lookupSchema(const AuditSchemaRegistry& registry,
                                     uint16_t category, uint16_t event_type);

/// Register a mod-defined event schema at runtime.
/// Returns false if a schema for the same (category, event_type) already
/// exists.
bool registerModSchema(AuditSchemaRegistry& registry,
                       const AuditSchemaEntry& entry);

/// Compute a hash of all registered schemas for forward-compatibility checks.
/// Written into .vxaudit file headers so the reader can detect schema drift.
uint64_t computeSchemaVersionHash(const AuditSchemaRegistry& registry);

}  // namespace eng
