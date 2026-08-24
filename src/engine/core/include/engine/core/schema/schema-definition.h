#pragma once

#include <engine/core/schema/schema-field-rule.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace eng::schema {

// ============================================================================
// DESIGN SUMMARY -- SchemaDefinition
// Technical Approach:
// docs/technical-approaches/engine/data-format-schemas/schema-infrastructure.md
//
// Holds all validation rules for a single schema (e.g. voxel_type, entity).
// Parsed from a .schema.json file. Contains top-level field rules and nested
// object rules keyed by JSON pointer prefix.
// ============================================================================

/// Complete validation rules for one data format schema.
/// Thread-safe: immutable after construction during schema loading.
struct SchemaDefinition {
  /// Schema identifier (e.g. "simplish/voxel_type/1.0").
  std::string schema_id{};

  /// Human-readable title (e.g. "VoxelType").
  std::string title{};

  /// Validation rules for top-level fields.
  std::vector<SchemaFieldRule> fields{};

  /// Validation rules for nested object fields.
  /// Key is the JSON pointer prefix (e.g. "physics", "render/pbr").
  std::unordered_map<std::string, std::vector<SchemaFieldRule>>
      nested_objects{};
};

}  // namespace eng::schema
