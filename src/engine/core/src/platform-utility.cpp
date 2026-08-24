#include <engine/core/assert.h>
#include <engine/core/platform-utility-impl.h>
#include <engine/core/platform-utility.h>
#include <string>

namespace eng {

// File-scoped impl table — written once at startup, read-only thereafter.
// Same justification as ThreadContext thread_local: write-once startup state.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static const PlatformUtilityImpl* s_impl = nullptr;

/// Assert that the impl table has been registered.
static void assertRegistered() {
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while)
  ENGINE_ASSERT(s_impl != nullptr,
                "PlatformUtility::registerImpl() not called");
}

void PlatformUtility::registerImpl(const PlatformUtilityImpl& impl) {
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while)
  ENGINE_ASSERT(s_impl == nullptr,
                "PlatformUtility::registerImpl() called twice");
  // Store address of caller-owned static — must outlive all calls.
  s_impl = &impl;
}

bool PlatformUtility::hasImpl() {
  return s_impl != nullptr;
}

void PlatformUtility::showOpenFolderDialog(const FolderDialogParams& params,
                                           FolderDialogCallback callback) {
  assertRegistered();
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while)
  ENGINE_ASSERT(s_impl->show_open_folder != nullptr,
                "show_open_folder not implemented");
  s_impl->show_open_folder(params, callback);
}

void PlatformUtility::showVirtualKeyboard() {
  assertRegistered();
  if (s_impl->show_virtual_keyboard != nullptr) {
    s_impl->show_virtual_keyboard();
  }
}

void PlatformUtility::hideVirtualKeyboard() {
  assertRegistered();
  if (s_impl->hide_virtual_keyboard != nullptr) {
    s_impl->hide_virtual_keyboard();
  }
}

bool PlatformUtility::isVirtualKeyboardVisible() {
  assertRegistered();
  if (s_impl->is_virtual_keyboard_visible == nullptr) {
    return false;
  }
  return s_impl->is_virtual_keyboard_visible();
}

bool PlatformUtility::hasNativeFileDialog() {
  assertRegistered();
  if (s_impl->has_native_file_dialog == nullptr) {
    return false;
  }
  return s_impl->has_native_file_dialog();
}

bool PlatformUtility::hasVirtualKeyboard() {
  assertRegistered();
  if (s_impl->has_virtual_keyboard == nullptr) {
    return false;
  }
  return s_impl->has_virtual_keyboard();
}

}  // namespace eng
