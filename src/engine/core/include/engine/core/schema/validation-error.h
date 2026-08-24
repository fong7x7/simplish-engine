#pragma once

#include <engine/core/schema/validation-error-kind.h>
#include <string>

namespace eng::schema {

// ============================================================================
// DESIGN SUMMARY -- ValidationError
// Technical Approach:
// docs/technical-approaches/engine/data-format-schemas/schema-infrastructure.md
//
// A single schema validation error with JSON pointer path, error kind, and
// human-readable message. Accumulated in ValidationResult.
// ============================================================================

/// A single schema validation error.
/// Thread-safe: immutable value type after construction.
struct ValidationError {
  /// JSON pointer path to the invalid field (e.g. "/physics/mass_per_voxel").
  std::string field_path{};

  /// Classification of this validation error.
  ValidationErrorKind kind = ValidationErrorKind::CUSTOM;

  /// Human-readable description of the error.
  std::string message{};
};

}  // namespace eng::schema
