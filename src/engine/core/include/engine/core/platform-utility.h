#pragma once

#include <string_view>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// PlatformUtility: Platform-agnostic interface for OS-level utilities.
//
// Responsibilities:
// - File dialogs (open folder, open file, save file)
// - Virtual keyboard (console platforms)
// - Capability queries (what the current platform supports)
//
// Key Invariants:
// - Implementation registered once at startup via registerImpl()
// - All functions are static — dispatch through a file-scoped impl table
// - Calling any function before registerImpl() is a fatal assertion
// - Write-once pattern: same justification as ThreadContext::get()
//
// Usage:
//   // At startup (platform layer):
//   PlatformUtility::registerImpl(desktop_impl);
//
//   // In editor/game code:
//   PlatformUtility::showOpenFolderDialog(params, my_callback);
//
// See ADR-010 for design rationale.
// Threading: Main-thread only.
// ============================================================================

/// Callback signature for folder dialog results.
/// @param userdata Opaque pointer forwarded from FolderDialogParams.
/// @param path Selected folder path, or nullptr if cancelled.
using FolderDialogCallback = void (*)(void* userdata, const char* path);

/// Platform-agnostic OS utility interface.
/// All functions dispatch through a function-pointer table registered at
/// startup. See ADR-010 for the dispatch pattern rationale.
/// @threading Main-thread only.
struct PlatformUtility {
  /// Parameters for an open-folder dialog request.
  /// @threading Main-thread only.
  struct FolderDialogParams {
    /// Opaque pointer forwarded to the callback.
    void* userdata = nullptr;
    /// Starting directory (empty = OS default).
    std::string_view default_path{};
  };

  // ─── Registration ───────────────────────────────────

  /// Install the platform implementation table.
  /// Must be called exactly once before any other PlatformUtility call.
  /// Forward-declares PlatformUtilityImpl to break include cycle:
  /// platform-utility-impl.h includes this header for callback types.
  static void registerImpl(const struct PlatformUtilityImpl& impl);

  /// True if an implementation has been registered.
  static bool hasImpl();

  // ─── File Dialogs ───────────────────────────────────

  /// Show a native folder-picker dialog (async — result via callback).
  static void showOpenFolderDialog(const FolderDialogParams& params,
                                   FolderDialogCallback callback);

  // ─── Virtual Keyboard ───────────────────────────────

  /// Show the on-screen keyboard (console platforms).
  static void showVirtualKeyboard();

  /// Hide the on-screen keyboard.
  static void hideVirtualKeyboard();

  /// Query whether the virtual keyboard is currently visible.
  static bool isVirtualKeyboardVisible();

  // ─── Capability Queries ─────────────────────────────

  /// True if the platform has a native file-picker dialog.
  static bool hasNativeFileDialog();

  /// True if the platform has a software virtual keyboard.
  static bool hasVirtualKeyboard();
};

}  // namespace eng
