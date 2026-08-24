#pragma once

#include <chrono>
#include <cstdint>
#include <engine/client/rendered-game-client.h>
#include <memory>
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
//   stays free of SDL includes
// - Initialize the engine with the SDL window as native handle
// - Create the RhiDevice via render::RhiDeviceFactory
// - Drive the loop: poll -> onTick -> presentGuiFrame
//
// Thread Safety:
// - Main thread only.
// ============================================================================

class DesktopGameClient : public eng::client::RenderedGameClient {
public:
  /// Whether an SDL key-down came from the first press or OS key-repeat.
  enum class ClientKeyDownKind : uint8_t {
    /// First `SDL_EVENT_KEY_DOWN` for this physical press.
    FIRST_PRESS,
    /// Auto-repeat while the key is held.
    REPEAT,
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
                               [[maybe_unused]] ClientKeyDownKind kind) {}

  /// Resize and input dispatch; subclasses that override must call this base
  /// implementation (or replicate resize, `dispatchSdlInputToGui`, and
  /// `onClientKeyDown`) so hooks stay wired.
  virtual void onEvent(const SDL_Event& event);

private:
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
};

}  // namespace eng::client
