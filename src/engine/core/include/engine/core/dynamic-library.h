#pragma once

#include <string>
#include <string_view>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// DynamicLibrary: Platform-agnostic wrapper for shared library operations.
//
// Abstracts dlopen/dlclose/dlsym (POSIX) and LoadLibrary/FreeLibrary/
// GetProcAddress (Win32) behind a uniform interface.
//
// Thread Safety:
// - Main thread only. All calls must happen on the main thread.
// ============================================================================

/// Platform-agnostic shared library operations.
/// Thread safety: main thread only.
struct DynamicLibrary {
  /// Load a shared library from the given path. Returns nullptr on failure.
  static void* open(std::string_view path);

  /// Unload a previously loaded shared library. No-op if handle is nullptr.
  static void close(void* handle);

  /// Look up a symbol by name in a loaded library. Returns nullptr if not
  /// found.
  static void* symbol(void* handle, std::string_view name);

  /// Get the last platform-specific error message from a failed operation.
  static std::string lastError();
};

}  // namespace eng
