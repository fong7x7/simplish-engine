#pragma once

#include <string_view>

namespace eng::schema {

// ============================================================================
// DESIGN SUMMARY -- SchemaRegistryContext
// Technical Approach:
// docs/technical-approaches/engine/data-format-schemas/schema-infrastructure.md
//
// Dependencies for loading schema files from disk. Passed as const& to
// SchemaRegistry::loadFromDir().
// ============================================================================

/// Dependencies for loading schema files.
/// Thread-safe: read-only context passed as const&.
struct SchemaRegistryContext {
  /// Path to the schemas directory (e.g. "data/schemas/").
  std::string_view schemas_dir{};
};

}  // namespace eng::schema
