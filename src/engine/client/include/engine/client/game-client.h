#pragma once

#include <cstdint>
#include <engine/client/game-client-config.h>
#include <engine/core/engine-context.h>
#include <memory>
#include <optional>
#include <string>

namespace eng::client {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// GameClient: Abstract base for the game client lifecycle.
//
// Responsibilities:
// - Define the platform lifecycle: init, run, shutdown
// - Define game-logic hooks: onInit, onTick, onShutdown
// - Own the engine context and track backbuffer dimensions
//
// Subclassing contract:
// - Platform subclasses (DesktopGameClient) implement init/run/shutdown
//   to manage the native window and event loop.
// - Game subclasses override onInit/onTick/onShutdown to inject game logic.
//   Desktop stacks: GameClient → RenderedGameClient (GUI + present) →
//   DesktopGameClient (SDL + RHI). Override the on* hooks on the leaf type.
//
// Key Invariants:
// - init() must be called before run() or shutdown()
// - run() blocks until onTick returns false or the platform requests quit
// - shutdown() must be called exactly once after run() completes
//
// Thread Safety:
// - All methods are main-thread-only.
// ============================================================================

class GameClient {
public:
  GameClient() = default;
  virtual ~GameClient() = default;

  GameClient(const GameClient&) = delete;
  GameClient& operator=(const GameClient&) = delete;
  GameClient(GameClient&&) = delete;
  GameClient& operator=(GameClient&&) = delete;

  /// Create the native window and initialize engine subsystems.
  /// Returns an error message on failure, nullopt on success.
  virtual std::optional<std::string> init(const GameClientConfig& config) = 0;

  /// Run the game loop until shutdown is requested.
  virtual void run() = 0;

  /// Shut down the engine and release platform resources.
  virtual void shutdown() = 0;

  /// Whether init() has completed successfully.
  [[nodiscard]] bool isInitialized() const { return initialized_; }

  /// Whether the game loop is currently running.
  [[nodiscard]] bool isRunning() const { return running_; }

  /// Drawable / swapchain width in pixels (may exceed window coordinate size on
  /// HiDPI displays).
  [[nodiscard]] uint32_t backbufferWidth() const { return backbuffer_width_; }

  /// Drawable / swapchain height in pixels (may exceed window coordinate size
  /// on HiDPI displays).
  [[nodiscard]] uint32_t backbufferHeight() const { return backbuffer_height_; }

  /// Access the engine context (non-null only after successful init).
  [[nodiscard]] EngineContext* engine() { return engine_.get(); }

protected:
  /// Called once after engine initialization succeeds.
  /// Return false to abort startup.
  virtual bool onInit() { return true; }

  /// Called once per frame with delta time in seconds.
  /// Return false to request shutdown.
  virtual bool onTick(float dt) = 0;

  /// Called once before engine shutdown for game-specific cleanup.
  virtual void onShutdown() {}

  /// Owned engine context (created during init).
  std::unique_ptr<EngineContext> engine_{};
  /// Drawable width in pixels (`SDL_GetWindowSizeInPixels` on desktop).
  uint32_t backbuffer_width_ = 0;
  /// Drawable height in pixels (`SDL_GetWindowSizeInPixels` on desktop).
  uint32_t backbuffer_height_ = 0;
  /// Whether the client has been initialized successfully.
  bool initialized_ = false;
  /// Whether the client is currently running.
  bool running_ = false;
};

}  // namespace eng::client
