#pragma once

#include "audit-types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// AuditErrorEntry: Definition of a single audit error code.
//
// Contains the numeric code, domain, symbolic name, severity, human-readable
// summary, detailed description, and cross-references. Loaded from JSON
// error code definitions.
//
// Thread Safety:
// - Value type; no thread-safety concerns.
// ============================================================================

struct AuditErrorEntry {
  /// Numeric error code (unique within the registry).
  uint32_t code{};
  /// Domain or subsystem that defines this error (e.g. "voxel", "physics").
  std::string domain;
  /// Symbolic name for the error code (e.g. "CHUNK_LOAD_FAILED").
  std::string symbol;
  /// Severity level of this error.
  AuditSeverity severity{AuditSeverity::TRACE};
  /// One-line summary description of the error.
  std::string summary;
  /// Detailed description with remediation guidance.
  std::string detail;
  /// Related error symbols or documentation references.
  std::vector<std::string> see_also;
};

}  // namespace eng
