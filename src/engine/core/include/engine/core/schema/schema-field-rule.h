#pragma once

#include <cstdint>
#include <engine/core/schema/schema-field-type.h>
#include <optional>
#include <string>
#include <vector>

namespace eng::schema {

// ============================================================================
// DESIGN SUMMARY -- SchemaFieldRule
// Technical Approach:
// docs/technical-approaches/engine/data-format-schemas/schema-infrastructure.md
//
// Represents one field's validation constraints parsed from a .schema.json
// file. Supports type checking, required flag, enum values, numeric ranges,
// string patterns, and array length bounds.
// ============================================================================

/// Whether a schema field is required or optional.
enum class FieldRequirement : uint8_t {
  /// Field may be absent from the JSON document.
  OPTIONAL = 0,
  /// Field must be present; absence is a blocking error.
  REQUIRED,
};

/// Validation constraints for a single JSON field.
/// Thread-safe: immutable after construction during schema loading.
struct SchemaFieldRule {
  /// JSON field name (leaf key, not full path).
  std::string field_name{};

  /// Expected JSON type for this field.
  SchemaFieldType type = SchemaFieldType::STRING;

  /// Whether this field must be present.
  FieldRequirement requirement = FieldRequirement::OPTIONAL;

  /// Allowed string values (empty = any string accepted).
  std::vector<std::string> enum_values{};

  /// Inclusive minimum for numeric fields.
  std::optional<double> minimum{};

  /// Inclusive maximum for numeric fields.
  std::optional<double> maximum{};

  /// Exclusive minimum for numeric fields.
  std::optional<double> exclusive_min{};

  /// Exclusive maximum for numeric fields.
  std::optional<double> exclusive_max{};

  /// Regex pattern for string fields.
  std::optional<std::string> pattern{};

  /// Minimum array length.
  std::optional<uint32_t> min_items{};

  /// Maximum array length.
  std::optional<uint32_t> max_items{};
};

}  // namespace eng::schema
