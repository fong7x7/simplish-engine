#pragma once

#include <engine/core/schema/schema-registry.h>
#include <engine/core/schema/validation-result.h>
#include <nlohmann/json_fwd.hpp>
#include <string_view>

namespace eng::schema {

// ============================================================================
// DESIGN SUMMARY -- SchemaValidator
// Technical Approach:
// docs/technical-approaches/engine/data-format-schemas/schema-infrastructure.md
//
// Behaviours:
//   - Validate a parsed JSON value against a named schema from the registry
//   - Check required fields, types, enum values, numeric ranges, patterns
//   - Validate nested objects recursively
//   - Accumulate all errors (never stops at first failure)
//   - Unknown fields are silently ignored (forward compatibility)
//
// Edge Cases:
//   - Schema not found in registry -> SCHEMA_VERSION_MISMATCH error
//   - Null JSON value -> WRONG_TYPE error for all required fields
//   - Empty JSON object -> MISSING_REQUIRED_FIELD for each required field
//
// Invariants:
//   - Pure function: no side effects, no mutation of inputs
//   - Thread-safe: reads only from immutable SchemaRegistry
// ============================================================================

/// Validates parsed JSON documents against schema definitions.
/// Thread-safe: all functions are pure (read-only access to registry).
struct SchemaValidator {
  /// Validate a parsed JSON value against the named schema.
  /// Returns a ValidationResult with all accumulated errors.
  static ValidationResult validate(const nlohmann::json& json,
                                   std::string_view schema_id,
                                   const SchemaRegistry& registry);
};

}  // namespace eng::schema
