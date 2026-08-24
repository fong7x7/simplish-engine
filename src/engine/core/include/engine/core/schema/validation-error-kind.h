#pragma once

#include <cstdint>

namespace eng::schema {

// ============================================================================
// DESIGN SUMMARY -- ValidationErrorKind
// Technical Approach:
// docs/technical-approaches/engine/data-format-schemas/schema-infrastructure.md
//
// Classifies schema validation failures. Used by ValidationError and
// ValidationResult to categorise errors for loaders and error display.
// ============================================================================

/// Classification of a schema validation error.
/// Thread-safe: enum is immutable value type.
enum class ValidationErrorKind : uint8_t {
  /// A field marked required in the schema is missing.
  MISSING_REQUIRED_FIELD = 0,

  /// A field value has the wrong JSON type (e.g. string where number expected).
  WRONG_TYPE,

  /// A string field value is not in the schema's allowed enum list.
  ENUM_VALUE_UNKNOWN,

  /// A numeric field value is outside the schema's declared range.
  OUT_OF_RANGE,

  /// A definition ID collides with an already-registered definition.
  DUPLICATE_ID,

  /// A cross-reference field points to a non-existent registry entry.
  INVALID_REFERENCE,

  /// The file's schema field does not match any loaded schema ID.
  SCHEMA_VERSION_MISMATCH,

  /// A validation rule that does not fit other categories.
  CUSTOM,
};

}  // namespace eng::schema
