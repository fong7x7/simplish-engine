#pragma once

#include <chrono>
#include <cstdint>
#include <engine/client/rendered-game-client.h>
#include <engine/gui/gui-gamepad-navigator.h>
#include <engine/gui/gui-nav-command.h>
#include <engine/input/gamepad-button.h>
#include <engine/input/gamepad-set.h>
#include <engine/input/gamepads.h>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

struct SDL_Window;
union SDL_Event;

namespace eng::client {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// DesktopGameClient: SDL3 window + engine + RHI on top of RenderedGameClient.
//
// Responsibilities:
// - Create and manage an SDL3 window
// - Poll SDL3 events; map SDL_EVENT_* to RenderedGameClient guiDispatch* calls
// - Expose windowClientSizePx() and onClientKeyDown() so game/editor code
//   stays free of SDL includes, modifier state included
// - Own the platform's pads (platform/input), poll them once a frame, and
//   raise onClientGamepadButtonDown for presses on the pad in use
// - When asked to, drive the GUI's focus navigation from the pad in use
// - Initialize the engine with the SDL window as native handle
// - Create the RhiDevice via render::RhiDeviceFactory
// - Drive the loop: poll -> onTick -> presentGuiFrame
//
// Thread Safety:
// - Main thread only.
// ============================================================================

class DesktopGameClient : public eng::client::RenderedGameClient {
public:
  /// Which dialog an answer came back from. One pending slot serves both,
  /// so the purpose is what routes the answer to the right handler.
  enum class DialogPurpose : uint8_t {
    /// A location and name for something to be created.
    SAVE_LOCATION,
    /// An existing folder.
    OPEN_FOLDER,
  };

  /// Whether the pad in use drives the GUI's focus navigation.
  enum class GuiPadNavigation : uint8_t {
    /// It does not: the game reads the pad, as it does in play.
    OFF,
    /// The d-pad or left stick moves focus, South confirms, East cancels.
    ON,
  };

  /// Whether an SDL key-down came from the first press or OS key-repeat.
  enum class ClientKeyDownKind : uint8_t {
    /// First `SDL_EVENT_KEY_DOWN` for this physical press.
    FIRST_PRESS,
    /// Auto-repeat while the key is held.
    REPEAT,
  };

  /// Modifier keys held when a key went down.
  ///
  /// Reported as the four physical groups rather than one "accelerator"
  /// flag: which of them a shortcut wants is the consumer's convention, not
  /// the platform's, and the engine GUI already accepts either Control or
  /// Command for its own clipboard keys on every platform.
  struct ClientKeyModifiers {
    /// Either Shift key.
    bool shift = false;
    /// Either Control key.
    bool ctrl = false;
    /// Either Alt/Option key.
    bool alt = false;
    /// The Command, Super, or Windows key.
    bool gui = false;
  };

  DesktopGameClient() = default;
  ~DesktopGameClient() override;

  std::optional<std::string>
  init(const eng::client::GameClientConfig& config) override;
  void run() override;
  void shutdown() override;

protected:
  /// Raw SDL window handle; null before init().
  [[nodiscard]] SDL_Window* window() const { return window_; }

  /// RHI device created by the platform factory; null before init().
  [[nodiscard]] eng::RhiDevice* rhiDevice() const override {
    return rhi_device_.get();
  }

  /// Current window client area in logical coordinates.
  [[nodiscard]] uint32_t guiLayoutWidth() const override;
  [[nodiscard]] uint32_t guiLayoutHeight() const override;

  /// HiDPI scale factor from SDL.
  [[nodiscard]] float textRasterSupersample() const override;

  /// Current SDL window client area in window coordinates (matches
  /// `SDL_GetWindowSize`; used for layout and input).
  [[nodiscard]] std::pair<int, int> windowClientSizePx() const;

  /// Update the OS window title. UTF-8 expected. No-op before the SDL
  /// window has been created (e.g. very early init / unit tests without
  /// an SDL window).
  void setWindowTitle(std::string_view utf8_title);

  /// After GUI key dispatch; `key` is the platform key symbol (SDL keycode).
  virtual void onClientKeyDown([[maybe_unused]] uint32_t key,
                               [[maybe_unused]] ClientKeyDownKind kind,
                               [[maybe_unused]] ClientKeyModifiers modifiers) {}

  /// A key was released; `key` is the platform key symbol. Paired with
  /// `onClientKeyDown` for whatever tracks held keys — a playtest's
  /// movement — rather than acting on presses alone.
  virtual void onClientKeyUp([[maybe_unused]] uint32_t key) {}

  /// The window lost keyboard focus. The releases of any keys held at that
  /// moment will never arrive, so whatever tracks held keys drops them here.
  virtual void onClientFocusLost() {}

  /// Every connected pad, as read at the start of this frame, in the
  /// engine's canonical buttons and axes. Which pads can appear here is
  /// the platform's pad backend's decision, not the caller's.
  [[nodiscard]] const eng::input::GamepadSet& gamepads() const {
    return gamepads_.pads();
  }

  /// @p button went down on the pad in use this frame — for menus and
  /// other things that act on a press. Held state is `gamepads()`.
  virtual void
  onClientGamepadButtonDown([[maybe_unused]] eng::input::GamepadButton button) {
  }

  /// Let the pad in use navigate the GUI, or stop it. Off by default: in
  /// play the pad steers a character, and a menu turns this on while it is
  /// open. Turning it off forgets whatever was held.
  void setGuiPadNavigation(GuiPadNavigation mode);

  /// A navigation command from the pad that the GUI did not use — most
  /// often CANCEL with no text field typing, which is the open menu's cue
  /// to close. Only raised while pad navigation is on.
  virtual void
  onClientGuiNavUnhandled([[maybe_unused]] eng::GuiNavCommand command) {}

  /// Resize and input dispatch; subclasses that override must call this base
  /// implementation (or replicate resize, `dispatchSdlInputToGui`, and
  /// `onClientKeyDown`) so hooks stay wired.
  virtual void onEvent(const SDL_Event& event);

  /// Open the OS "save as" dialog so the user can pick a folder and type a
  /// name, starting in Documents (or home, or wherever the OS prefers).
  ///
  /// Returns immediately: the dialog is modal to the window but answers
  /// asynchronously, and SDL may run its callback on another thread. The
  /// answer arrives on the main thread through `onSaveLocationChosen`,
  /// before the next `onTick`. A cancelled dialog reports nothing at all.
  void showSaveLocationDialog();

  /// Open the OS folder picker, starting in the same place. Answers through
  /// `onFolderChosen`, with the same timing and cancellation behaviour.
  void showOpenFolderDialog();

  /// Called on the main thread with a chosen save location. Default no-op.
  virtual void onSaveLocationChosen(const std::filesystem::path& /*path*/) {}

  /// Called on the main thread with a chosen existing folder. Default no-op.
  virtual void onFolderChosen(const std::filesystem::path& /*path*/) {}

private:
  /// Raise `onClientKeyDown`, `onClientKeyUp` or `onClientFocusLost` for
  /// whichever of them @p event is, after the GUI has had it.
  void dispatchClientKey(const SDL_Event& event);

  /// Start the pad backend, logging rather than failing when it cannot.
  void openGamepads();

  /// Read the pads, raise `onClientGamepadButtonDown` for each press, and
  /// navigate the GUI by them when that is on, @p dt_seconds after the
  /// last frame.
  void pollGamepads(float dt_seconds);

  /// Feed the pad in use through the GUI navigator into the widget tree.
  void navigateGuiByPad(float dt_seconds);

  /// Tell the pads when @p event is the window gaining or losing focus.
  void trackGamepadFocus(const SDL_Event& event);

  /// Hand any pending dialog answer to the handler its purpose names.
  void drainDialogPath();

  /// Store a dialog answer for the main thread to pick up. Called from
  /// whichever thread SDL runs the dialog callback on.
  void storeDialogPath(DialogPurpose purpose, const char* path);

  /// Create SDL window and query initial backbuffer size.
  std::optional<std::string>
  initSdlWindow(const eng::client::GameClientConfig& config);

  /// Create the engine context from the game client config.
  std::optional<std::string>
  initEngineContext(const eng::client::GameClientConfig& cfg);

  /// Create the RHI device from the current window and backbuffer size.
  std::optional<std::string> initRhiDevice();

  /// Execute one iteration of the main loop; returns false to exit.
  bool runOneFrame(std::chrono::steady_clock::time_point& last_frame);

  /// Poll all pending SDL events.  Returns false on quit.
  bool pollEvents();

  /// SDL event types -> `guiDispatch*` (named mapping; SDL-specific).
  void dispatchSdlInputToGui(const SDL_Event& event);

  /// Start/stop SDL text input to match GUI focus state.
  void syncSdlTextInputState();

  /// Apply a new backbuffer size to RHI swapchain and GUI surface.
  void applyBackbufferResize(uint32_t w, uint32_t h);

  /// Refresh pixel backbuffer size, RHI swapchain, and GUI surface from SDL.
  void syncPixelBackbufferFromWindow();

  /// SDL window handle; null before init(), destroyed in shutdown().
  SDL_Window* window_ = nullptr;
  /// RHI device; created in init() via RhiDeviceFactory.
  std::unique_ptr<eng::RhiDevice> rhi_device_{};
  /// The platform's pads; opened in init(), closed in shutdown().
  eng::input::Gamepads gamepads_{};
  /// Turns the pad in use into GUI navigation commands.
  eng::GuiGamepadNavigator gui_navigator_{};
  /// Whether it runs.
  GuiPadNavigation gui_pad_navigation_ = GuiPadNavigation::OFF;
  /// Guards `pending_dialog_path_` against the dialog callback's thread.
  std::mutex dialog_mutex_{};
  /// Path chosen but not yet handed to the main thread, and what it is for.
  std::optional<std::pair<DialogPurpose, std::string>> pending_dialog_path_{};
};

}  // namespace eng::client
