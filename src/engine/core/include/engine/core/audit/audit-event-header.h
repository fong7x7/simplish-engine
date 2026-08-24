#pragma once

#include "audit-types.h"

#include <cstdint>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// AuditEventHeader: Fixed 40-byte binary header for audit events.
//
// Wire format uses little-endian byte order. Fields are ordered to avoid
// internal padding on LP64 and LLP64 platforms. Every audit event in the
// ring buffer and .vxaudit files begins with this header.
//
// Thread Safety:
// - POD type; no thread-safety concerns.
// ============================================================================

// Wire format: 40 bytes, little-endian.
// Layout chosen to avoid internal padding on LP64 and LLP64 platforms.
// Fields are ordered to maintain natural alignment.
struct AuditEventHeader {
  /// Monotonic nanoseconds from session start (offset 0).
  uint64_t timestamp_ns{};
  /// Engine frame counter at event emission time (offset 8).
  uint64_t frame_number{};
  /// Entity or user that caused the event; -1 for system (offset 16).
  int64_t actor_id{};
  /// Target entity; -1 if none (offset 24).
  int64_t target_id{};
  /// Event category ID (offset 32).
  uint16_t category{};
  /// Event type ID within the category (offset 34).
  uint16_t event_type{};
  /// Byte length of the variable payload (0-65535) (offset 36).
  uint16_t payload_size{};
  /// Severity level of this event (offset 38).
  AuditSeverity severity{};
  /// Bit flags for optional features (offset 39).
  AuditFlags flags{};
};

static_assert(sizeof(AuditEventHeader) == 40,
              "AuditEventHeader must be 40 bytes");

// --- Constants ---

inline constexpr uint16_t AUDIT_MAX_PAYLOAD_SIZE = 65535;
inline constexpr int64_t AUDIT_ACTOR_SYSTEM = -1;
inline constexpr int64_t AUDIT_TARGET_NONE = -1;
inline constexpr uint32_t AUDIT_MOD_ERROR_CODE_MIN = 100000;

}  // namespace eng
