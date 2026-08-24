#pragma once

// Design Summary -- Epic Cloud Storage
// Technical Approach:
//   docs/technical-approaches/engine/platform-epic/cloud-storage.md
//
// Behaviours:
//   - Write/read/delete files in EOS Player Data Storage (async)
//   - Query list of cloud files for authenticated user
//   - Data encrypted at rest via encryption_key from config
//
// Edge Cases:
//   - EOS not available: callback receives error
//   - File not found on read: callback receives CLOUD_FILE_NOT_FOUND
//   - Write exceeds quota: callback receives CLOUD_QUOTA_EXCEEDED
//   - Empty data write: callback receives INVALID_ARGUMENT
//
// Invariants:
//   - All cloud operations are async; completion on main thread
//   - Cloud functions are main-thread-only
//   - No EOS SDK types in public API
//
// Integration Points:
//   - World Storage: cloud save sync for .vxworld files

#include "epic-types.h"

#include <cstddef>
#include <engine/core/expected-polyfill.h>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace eng {

// Forward declarations
struct EpicContext;

// ---------------------------------------------------------------------------
// Callback types
// ---------------------------------------------------------------------------

using EpicCloudWriteCallback =
    std::function<void(std::string_view filename, EpicError error)>;

using EpicCloudReadCallback = std::function<void(
    std::string_view filename,
    std::expected<std::vector<std::byte>, EpicError> result)>;

using EpicCloudDeleteCallback =
    std::function<void(std::string_view filename, EpicError error)>;

using EpicCloudQueryCallback = std::function<void(
    std::expected<std::vector<std::string>, EpicError> result)>;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Write data to EOS Player Data Storage. Async; callback fires on
/// completion during EOS_Platform_Tick(). Returns INVALID_ARGUMENT
/// if filename is empty or data is empty.
/// Main thread only.
void epicCloudWrite(const EpicContext& ctx, std::string_view filename,
                    std::span<const std::byte> data,
                    const EpicCloudWriteCallback& callback);

/// Read a file from EOS Player Data Storage. Async; callback
/// receives the file data or an error. Chunks are reassembled
/// internally before invoking callback.
/// Main thread only.
void epicCloudRead(const EpicContext& ctx, std::string_view filename,
                   const EpicCloudReadCallback& callback);

/// Delete a file from EOS Player Data Storage. Async.
/// Main thread only.
void epicCloudDelete(const EpicContext& ctx, std::string_view filename,
                     const EpicCloudDeleteCallback& callback);

/// Query the list of all cloud files for the authenticated user.
/// Async; callback receives filenames or error.
/// Main thread only.
void epicCloudQueryFiles(const EpicContext& ctx,
                         const EpicCloudQueryCallback& callback);

}  // namespace eng
