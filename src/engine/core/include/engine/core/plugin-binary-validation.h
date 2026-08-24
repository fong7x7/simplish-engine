#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// PluginBinaryValidation: Validate a plugin shared library before loading.
//
// Opens the binary via DynamicLibrary, resolves all required entry point
// symbols, and checks that the reported API version matches the engine's
// expected version. On failure, the dlhandle is closed and an error message
// is populated.
//
// Shared by both initial plugin loading and hot-reload validation.
//
// Thread Safety:
// - Main thread only. All calls must happen on the main thread.
// ============================================================================

/// Result of validating a plugin binary.
/// Thread safety: main thread only (owns a dlhandle).
struct PluginBinaryValidation {
  /// Handle from DynamicLibrary::open (nullptr on failure).
  void* dlhandle = nullptr;
  /// API version reported by the binary (0 if not resolved).
  uint32_t api_version = 0;
  /// Error description; empty string on success.
  std::string error_message{};
};

/// Open and validate a plugin binary: resolve required symbols and check
/// API version. On failure, the returned validation has dlhandle == nullptr
/// and error_message populated.
/// Thread safety: main thread only.
PluginBinaryValidation validatePluginBinary(std::string_view binary_path,
                                            uint32_t expected_api_version);

/// Close a validated binary handle. Safe to call on failed validations
/// (no-op if dlhandle is nullptr).
/// Thread safety: main thread only.
void closeValidatedBinary(PluginBinaryValidation& validation);

}  // namespace eng
