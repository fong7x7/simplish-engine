#pragma once

#include "audit-session-metadata.h"

#include <memory>
#include <string>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// AuditFileReader: Handle for reading .vxaudit files for offline analysis.
//
// Contains the file path, parsed session metadata, and an opaque pointer
// to implementation details (file handle, read cursor, decompression state).
//
// Thread Safety:
// - Not thread-safe; use from a single thread.
// ============================================================================

/// Forward declaration of the opaque implementation type defined in
/// audit-persistence.cpp.
struct AuditFileReaderImpl;  // NOLINT(no-forward-decl) opaque Impl pointer

struct AuditFileReader {
  /// Filesystem path of the .vxaudit file being read.
  std::string path;
  /// Session metadata parsed from the file header.
  AuditSessionMetadata metadata;
  /// Opaque implementation details (file handle, read cursor). Owned by this
  /// struct; destructor defined in audit-persistence.cpp where the type is
  /// complete.
  std::unique_ptr<AuditFileReaderImpl> impl;

  AuditFileReader();
  ~AuditFileReader();
  AuditFileReader(AuditFileReader&&) noexcept;
  AuditFileReader& operator=(AuditFileReader&&) noexcept;
};

}  // namespace eng
