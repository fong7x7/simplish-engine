#pragma once

#include "audit-types.h"

#include <cstdint>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// AuditConfig: Configuration for the audit system.
//
// Controls buffer sizing, persistence, severity filtering, network sync,
// and Event Log Viewer limits. Loaded from config/audit.toml at engine init.
//
// Thread Safety:
// - Value type; read-only after init.
// ============================================================================

struct AuditConfig {
  /// Master toggle for the audit system.
  bool enabled = true;
  /// Per-thread ring buffer size in megabytes.
  uint32_t ring_buffer_size_mb = 64;
  /// Whether to flush events to .vxaudit files on disk.
  bool persist_to_disk = true;
  /// Directory path for .vxaudit output files.
  const char* output_directory = "temp/audit_logs";
  /// Maximum total disk usage for audit files in megabytes.
  uint32_t max_disk_mb = 512;
  /// Buffer fullness ratio that triggers a disk flush (0.0 to 1.0).
  float flush_threshold = 0.75f;
  /// Minimum severity level; events below this are discarded.
  AuditSeverity min_severity = AuditSeverity::TRACE;
  /// Whether to sync audit events over the network.
  bool network_sync_enabled = false;
  /// Whether to include events received from remote peers.
  bool include_remote_events = true;
  /// Maximum events shown in the Event Log Viewer.
  uint32_t max_visible_events = 10000;
};

}  // namespace eng
