#pragma once

#include "audit-event-header.h"

#include <cstddef>
#include <span>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// AuditEventRecord: Read-only view of a completed audit event.
//
// Contains the fixed-size header and a span over the variable-length payload.
// Used by query results and real-time event subscriptions.
//
// Thread Safety:
// - Read-only view; safe from any thread as long as the underlying
//   ring buffer memory is valid.
// ============================================================================

struct AuditEventRecord {
  /// Fixed-size 40-byte event header with metadata.
  AuditEventHeader header;
  /// View into the variable-length payload bytes following the header.
  std::span<const std::byte> payload;
};

}  // namespace eng
