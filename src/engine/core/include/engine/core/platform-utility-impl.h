#pragma once

#include <engine/core/platform-utility.h>

namespace eng {

/// Function-pointer dispatch table for PlatformUtility.
/// Populated by the platform layer and registered via
/// PlatformUtility::registerImpl() at startup.
/// @threading Main-thread only — set once before any calls.
struct PlatformUtilityImpl {
  // ─── Type aliases ───────────────────────────────────

  /// Signature for showOpenFolderDialog implementation.
  using ShowFolderFn = void (*)(const PlatformUtility::FolderDialogParams&,
                                FolderDialogCallback);
  /// Signature for void(void) implementations.
  using VoidFn = void (*)();
  /// Signature for bool(void) implementations.
  using BoolFn = bool (*)();

  // ─── Function pointers ─────────────────────────────

  /// Implementation for PlatformUtility::showOpenFolderDialog.
  ShowFolderFn show_open_folder = nullptr;
  /// Implementation for PlatformUtility::showVirtualKeyboard.
  VoidFn show_virtual_keyboard = nullptr;
  /// Implementation for PlatformUtility::hideVirtualKeyboard.
  VoidFn hide_virtual_keyboard = nullptr;
  /// Implementation for PlatformUtility::isVirtualKeyboardVisible.
  BoolFn is_virtual_keyboard_visible = nullptr;
  /// Implementation for PlatformUtility::hasNativeFileDialog.
  BoolFn has_native_file_dialog = nullptr;
  /// Implementation for PlatformUtility::hasVirtualKeyboard.
  BoolFn has_virtual_keyboard = nullptr;
};

}  // namespace eng
