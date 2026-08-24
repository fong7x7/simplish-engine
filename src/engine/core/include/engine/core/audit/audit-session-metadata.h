#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// AuditSessionMetadata: Session information written to .vxaudit file headers.
//
// Captures the engine version, platform, map name, active mods, session
// start time, and schema version hash. Used for offline analysis and
// forward-compatibility checks.
//
// Thread Safety:
// - Value type; no thread-safety concerns.
// ============================================================================

struct AuditSessionMetadata {
  /// Engine version string (e.g. "0.1.0-alpha").
  std::string engine_version;
  /// Platform identifier (e.g. "win64", "macos-arm64", "ps5").
  std::string platform;
  /// Name of the map or world being played.
  std::string map_name;
  /// List of active mod IDs for this session.
  std::vector<std::string> active_mods;
  /// Session start time in monotonic nanoseconds.
  uint64_t start_timestamp_ns{};
  /// Hash of the schema registry at session start for compatibility checks.
  uint64_t schema_version_hash{};
};

}  // namespace eng
