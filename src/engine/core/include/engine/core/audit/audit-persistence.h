#pragma once

#include "audit-config.h"
#include "audit-file-reader.h"
#include "audit-session-metadata.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace eng {

// ============================================================================
// DESIGN SUMMARY -- Audit Persistence
// Technical Approach:
// docs/technical-approaches/engine/audit/ring-buffer-persistence.md
//
// Behaviours:
//   - Write .vxaudit files: magic VXAU, version, session metadata, LZ4 blocks
//   - Flush events in 256 KB raw blocks compressed with LZ4
//   - Automatic flush at flush_threshold and on session end
//   - Configurable output directory and max disk usage
//   - Prune oldest files when total size exceeds max_disk_mb
//   - Read .vxaudit files for offline analysis (standalone viewer)
//
// Edge Cases:
//   - Disk write failure: log error, continue; events stay in ring buffer
//   - LZ4 compression failure: write uncompressed block as fallback
//   - Corrupted .vxaudit on read: skip bad blocks, load valid remainder
//   - Output directory missing: create it; if fails, disable persistence
//   - Zero events to flush: no-op
//
// Invariants:
//   - .vxaudit files are self-contained (header + compressed blocks)
//   - Disk I/O never blocks the game thread (runs on background thread)
//   - File format uses little-endian byte order
//
// Integration Points:
//   - audit-ring-buffer.h: merge buffer provides raw events for flushing
//   - audit-schema.h: schema version hash written to file header
//   - audit-system.h: owns AuditPersistenceContext lifecycle
// ============================================================================

// --- File format constants ---

inline constexpr uint32_t AUDIT_FILE_MAGIC =
    0x55415856;  // "VXAU" little-endian
inline constexpr uint32_t AUDIT_FILE_VERSION = 1;
inline constexpr uint32_t AUDIT_BLOCK_SIZE =
    256 * 1024;  // 256 KB raw per block

// --- File handle (opaque; implementation in .cpp) ---

struct AuditFileHandle;  // NOLINT(no-forward-decl) opaque Impl pointer

// --- Persistence context ---

struct AuditPersistenceContext {
  /// Default constructor.
  AuditPersistenceContext();
  /// Destructor — defined in .cpp where AuditFileHandle is complete.
  ~AuditPersistenceContext();
  /// Move constructor.
  AuditPersistenceContext(AuditPersistenceContext&&) noexcept;
  /// Move assignment.
  AuditPersistenceContext& operator=(AuditPersistenceContext&&) noexcept;
  AuditPersistenceContext(const AuditPersistenceContext&) = delete;
  AuditPersistenceContext& operator=(const AuditPersistenceContext&) = delete;

  /// Directory where .vxaudit files are written.
  std::string output_directory;
  /// Maximum total disk usage in bytes before pruning old files.
  uint64_t max_disk_bytes = 0;
  /// Ring buffer fullness ratio that triggers a flush (0.0 to 1.0).
  float flush_threshold = 0.75f;
  /// RAII handle to the currently open .vxaudit file (nullptr if none).
  std::unique_ptr<AuditFileHandle> current_file;
  /// Current size in bytes of the open .vxaudit file.
  uint64_t current_file_size = 0;
  /// Total disk usage in bytes across all .vxaudit files.
  uint64_t total_disk_usage = 0;
};

// --- Public API ---

/// Create a persistence context from configuration.
/// Ensures the output directory exists (creates if necessary).
/// Returns nullopt if the directory cannot be created.
std::optional<AuditPersistenceContext>
createPersistenceContext(const AuditConfig& config);

/// Open a new .vxaudit file for the current session.
/// Writes the file header with magic, version, and session metadata.
/// Returns false if the file cannot be created.
bool openAuditFile(AuditPersistenceContext& ctx,
                   const AuditSessionMetadata& metadata);

/// Flush raw events to disk as an LZ4-compressed block.
/// Called by the merge thread when buffer fullness exceeds flush_threshold.
/// Returns false on write failure (events remain in ring buffer).
bool flushToDisk(AuditPersistenceContext& ctx,
                 std::span<const std::byte> raw_events);

/// Close the current .vxaudit file.
/// Flushes any remaining buffered data.
void closeAuditFile(AuditPersistenceContext& ctx);

/// Remove oldest .vxaudit files until total disk usage is within
/// max_disk_bytes.
void pruneOldFiles(AuditPersistenceContext& ctx);

/// Open a .vxaudit file for reading (offline analysis).
/// Returns nullopt if the file is missing, has invalid magic, or version
/// mismatch.
std::optional<AuditFileReader> openAuditFileForRead(std::string_view path);

/// Read and decompress the next block from an open .vxaudit file.
/// Returns nullopt when all blocks have been read or on decompression error.
std::optional<std::vector<std::byte>> readNextBlock(AuditFileReader& reader);

/// Close a file reader and free resources.
void closeAuditFileReader(AuditFileReader& reader);

/// Shut down persistence: close open file, free resources.
void shutdownPersistence(AuditPersistenceContext& ctx);

}  // namespace eng
