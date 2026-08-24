#pragma once

#include <engine/core/schema/validation-error-kind.h>
#include <engine/core/schema/validation-error.h>
#include <string>
#include <vector>

namespace eng::schema {

// ============================================================================
// DESIGN SUMMARY -- ValidationResult
// Technical Approach:
// docs/technical-approaches/engine/data-format-schemas/schema-infrastructure.md
//
// Accumulates zero or more ValidationError entries during schema validation.
// Validation does not stop at the first failure — all errors are collected.
// Blocking errors (MISSING_REQUIRED_FIELD, SCHEMA_VERSION_MISMATCH) cause
// the loader to skip the definition entirely.
// ============================================================================

/// Accumulated schema validation errors for a single JSON document.
/// Thread-safe: only used by the validating thread; not shared.
struct ValidationResult {
  /// All accumulated validation errors.
  std::vector<ValidationError> errors{};

  /// True if any error exists.
  static bool hasErrors(const ValidationResult& result);

  /// True if any blocking error exists (MISSING_REQUIRED_FIELD or
  /// SCHEMA_VERSION_MISMATCH).
  static bool hasBlockingErrors(const ValidationResult& result);

  /// Append an error to the result.
  static void addError(ValidationResult& result, std::string field_path,
                       ValidationErrorKind kind, std::string message);
};

}  // namespace eng::schema
