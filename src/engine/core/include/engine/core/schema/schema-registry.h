#pragma once

#include <engine/core/schema/schema-definition.h>
#include <engine/core/schema/schema-registry-context.h>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace eng::schema {

// ============================================================================
// DESIGN SUMMARY -- SchemaRegistry
// Technical Approach:
// docs/technical-approaches/engine/data-format-schemas/schema-infrastructure.md
//
// Behaviours:
//   - Load all .schema.json files from a directory at engine init
//   - Store SchemaDefinition structs keyed by schema_id
//   - Provide O(1) lookup by schema_id
//
// Edge Cases:
//   - Directory missing -> return nullopt, engine continues with empty registry
//   - Malformed schema file -> skip, log warning, continue with others
//   - Duplicate schema_id -> last loaded wins, log warning
//
// Invariants:
//   - Immutable after loadFromDir() completes (thread-safe for reads)
//   - Must be loaded before any data definition loading begins
// ============================================================================

/// Registry of all loaded schema definitions, keyed by schema_id.
/// Thread-safe: immutable after construction via loadFromDir().
struct SchemaRegistry {
  /// All loaded schema definitions keyed by schema_id.
  std::unordered_map<std::string, SchemaDefinition> schemas{};

  /// Load all .schema.json files from the schemas directory.
  /// Returns nullopt if the directory cannot be opened.
  static std::optional<SchemaRegistry>
  loadFromDir(const SchemaRegistryContext& ctx);

  /// Find a schema definition by its schema_id.
  /// Returns nullptr if not found.
  static const SchemaDefinition* findSchema(const SchemaRegistry& registry,
                                            std::string_view schema_id);
};

}  // namespace eng::schema
