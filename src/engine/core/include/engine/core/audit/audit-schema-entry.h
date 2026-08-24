#pragma once

#include "audit-schema-field.h"

#include <cstdint>
#include <vector>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// AuditSchemaEntry: Schema definition for a single (category, event_type) pair.
//
// Contains an ordered list of AuditSchemaField descriptors that match the
// binary payload layout. Used for payload serialization, deserialization,
// and human-readable summary generation.
//
// Thread Safety:
// - Value type; no thread-safety concerns.
// ============================================================================

struct AuditSchemaEntry {
  /// Event category ID.
  uint16_t category{};
  /// Event type ID within the category.
  uint16_t event_type{};
  /// Ordered list of payload fields matching the binary layout.
  std::vector<AuditSchemaField> fields;
};

}  // namespace eng
