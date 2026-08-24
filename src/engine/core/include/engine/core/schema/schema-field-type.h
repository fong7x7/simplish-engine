#pragma once

#include <cstdint>

namespace eng::schema {

// ============================================================================
// DESIGN SUMMARY -- SchemaFieldType
// Technical Approach:
// docs/technical-approaches/engine/data-format-schemas/schema-infrastructure.md
//
// JSON type discriminator for schema field rules. Maps to nlohmann::json
// type checks (is_string, is_number_integer, is_number, is_boolean,
// is_object, is_array).
// ============================================================================

/// Expected JSON type for a schema field.
enum class SchemaFieldType : uint8_t {
  /// JSON string value.
  STRING = 0,

  /// JSON integer value (no fractional part).
  INTEGER,

  /// JSON number value (integer or floating-point).
  NUMBER,

  /// JSON boolean value.
  BOOLEAN,

  /// JSON object value.
  OBJECT,

  /// JSON array value.
  ARRAY,
};

}  // namespace eng::schema
