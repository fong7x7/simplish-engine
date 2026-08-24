#include <engine/client/desktop-platform-utility.h>
#include <engine/core/platform-utility-impl.h>
#include <engine/core/platform-utility.h>
#include <memory>
#include <string>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#include <SDL3/SDL.h>
#pragma clang diagnostic pop

namespace eng::client {

/// Bridging context passed through SDL's void* userdata.
/// @threading Main-thread only.
struct SdlFolderBridge {
  /// Original callback to invoke with the selected path.
  FolderDialogCallback callback = nullptr;
  /// Original userdata to forward.
  void* userdata = nullptr;
};

/// SDL3 callback adapter — translates filelist to single path.
static void sdlFolderCallback(void* userdata, const char* const* filelist,
                              int /*filter*/) {
  auto bridge =
      std::unique_ptr<SdlFolderBridge>(static_cast<SdlFolderBridge*>(userdata));
  const char* path = nullptr;
  if (filelist != nullptr && filelist[0] != nullptr) {
    path = filelist[0];
  }
  bridge->callback(bridge->userdata, path);
}

/// SDL3 implementation of showOpenFolderDialog.
static void showOpenFolder(const PlatformUtility::FolderDialogParams& params,
                           FolderDialogCallback callback) {
  auto bridge = std::make_unique<SdlFolderBridge>(
      SdlFolderBridge{.callback = callback, .userdata = params.userdata});
  const char* default_path =
      params.default_path.empty() ? nullptr : params.default_path.data();
  constexpr bool ALLOW_MANY = false;
  SDL_ShowOpenFolderDialog(sdlFolderCallback, bridge.release(), nullptr,
                           default_path, ALLOW_MANY);
}

static void noopVoid() {}

static bool returnFalse() {
  return false;
}

static bool returnTrue() {
  return true;
}

/// Static impl table — lives for the process lifetime.
static const eng::PlatformUtilityImpl DESKTOP_IMPL{
    .show_open_folder = showOpenFolder,
    .show_virtual_keyboard = noopVoid,
    .hide_virtual_keyboard = noopVoid,
    .is_virtual_keyboard_visible = returnFalse,
    .has_native_file_dialog = returnTrue,
    .has_virtual_keyboard = returnFalse,
};

void DesktopPlatformUtility::install() {
  eng::PlatformUtility::registerImpl(DESKTOP_IMPL);
}

}  // namespace eng::client
