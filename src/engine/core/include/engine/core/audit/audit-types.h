#pragma once

#include <cstdint>

namespace eng {

// ============================================================================
// DESIGN SUMMARY -- Audit Types
// Technical Approach: docs/technical-approaches/engine/audit.md
//
// Behaviours:
//   - Define AuditSeverity enum (Trace, Debug, Info, Warn, Error)
//   - Define AuditFlags bitmask (HAS_POSITION, HAS_CAUSE, EDITOR_ORIGIN, etc.)
//   - Define AuditCategory constants for all game, editor, and system
//   categories
//   - Define AuditFieldType enum for payload schema field types
//
// Edge Cases:
//   - Severity below minimum: emit is skipped
//   - Category disabled: emit is skipped
//
// Invariants:
//   - All enums use fixed-width backing types for binary compatibility
//
// Integration Points:
//   - audit-event-header.h: uses AuditSeverity, AuditFlags
//   - audit-config.h: uses AuditSeverity
//   - audit-emit.h: uses AuditSeverity, AuditFlags, AuditCategory
//   - audit-schema.h: uses AuditFieldType
//   - audit-ring-buffer.h: uses AuditConfig for buffer sizing
// ============================================================================

// --- Severity levels ---

enum class AuditSeverity : uint8_t {
  TRACE = 0,
  DEBUG = 1,
  INFO = 2,
  WARN = 3,
  ERROR = 4,
};

// --- Event flags (bitmask) ---

enum class AuditFlags : uint8_t {
  NONE = 0,
  HAS_POSITION = 1 << 0,
  HAS_CAUSE = 1 << 1,
  EDITOR_ORIGIN = 1 << 2,
  NETWORK_REPLICATED = 1 << 3,
  PLUGIN_ORIGIN = 1 << 4,
  HAS_ERROR = 1 << 5,
};

/// Bitwise OR for combining flags.
inline AuditFlags operator|(AuditFlags lhs, AuditFlags rhs) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) bitmask enum
  return static_cast<AuditFlags>(static_cast<uint8_t>(lhs) |
                                 static_cast<uint8_t>(rhs));
}

/// Bitwise AND for testing flags.
inline AuditFlags operator&(AuditFlags lhs, AuditFlags rhs) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) bitmask enum
  return static_cast<AuditFlags>(static_cast<uint8_t>(lhs) &
                                 static_cast<uint8_t>(rhs));
}

/// Test whether a flag is set.
inline bool hasFlag(AuditFlags flags, AuditFlags test) {
  return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(test)) != 0;
}

// --- Event categories (uint16 IDs matching data/audit/event_categories.json)
// ---

namespace AuditCategory {

  // Game runtime
  inline constexpr uint16_t PHYSICS = 0x0100;
  inline constexpr uint16_t INTELLIGENCE_GOALS = 0x0200;
  inline constexpr uint16_t INTELLIGENCE_NEEDS = 0x0201;
  inline constexpr uint16_t INTELLIGENCE_EMOTIONS = 0x0202;
  inline constexpr uint16_t INTELLIGENCE_RELATIONSHIPS = 0x0203;
  inline constexpr uint16_t INTELLIGENCE_DIALOG = 0x0204;
  inline constexpr uint16_t INTELLIGENCE_NAVIGATION = 0x0205;
  inline constexpr uint16_t INTELLIGENCE_STEALTH = 0x0206;
  inline constexpr uint16_t INTELLIGENCE_CRIME = 0x0207;
  inline constexpr uint16_t INTELLIGENCE_GOSSIP = 0x0208;
  inline constexpr uint16_t INTELLIGENCE_MORALITY = 0x0209;
  inline constexpr uint16_t INTELLIGENCE_INVENTORY = 0x020A;
  inline constexpr uint16_t INTELLIGENCE_STATS = 0x020B;
  inline constexpr uint16_t ITEMS = 0x0300;
  inline constexpr uint16_t ANIMATION = 0x0400;
  inline constexpr uint16_t AUDIO = 0x0500;
  inline constexpr uint16_t VOXEL = 0x0600;
  inline constexpr uint16_t WORLD = 0x0700;
  inline constexpr uint16_t NETWORK = 0x0800;
  inline constexpr uint16_t SCRIPTING = 0x0900;
  inline constexpr uint16_t ENTITY = 0x0A00;
  inline constexpr uint16_t PLAYER = 0x0B00;

  // Editor
  inline constexpr uint16_t EDITOR_MAP = 0xE000;
  inline constexpr uint16_t EDITOR_CHARACTER = 0xE100;
  inline constexpr uint16_t EDITOR_DIALOG = 0xE200;
  inline constexpr uint16_t EDITOR_SCENARIO = 0xE300;
  inline constexpr uint16_t EDITOR_SHELL = 0xE400;
  inline constexpr uint16_t EDITOR_COLLAB = 0xE500;
  inline constexpr uint16_t EDITOR_AI = 0xE600;

  // System
  inline constexpr uint16_t SYSTEM_LIFECYCLE = 0xF000;
  inline constexpr uint16_t SYSTEM_PERFORMANCE = 0xF100;
  inline constexpr uint16_t SYSTEM_ERROR = 0xF200;

}  // namespace AuditCategory

// --- Payload field types (for schema definitions) ---

enum class AuditFieldType : uint8_t {
  UINT8,
  UINT16,
  UINT32,
  UINT64,
  INT8,
  INT16,
  INT32,
  INT64,
  FLOAT32,
  FLOAT64,
  BOOL,
  VEC3,
  QUAT,
  COLOUR,
  STRING16,
  BYTES,
  ENTITY_ID,
  ITEM_ID,
  ERROR_CODE,
};

// --- Timestamp function type (injectable for testing) ---

using TimestampFn = uint64_t (*)();

}  // namespace eng
