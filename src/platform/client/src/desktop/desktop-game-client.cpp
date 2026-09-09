#include "engine/client/desktop-game-client.h"

#include <algorithm>
#include <chrono>
#include <engine/client/desktop-dialog-start-folder.h>
#include <engine/client/desktop-platform-keycode.h>
#include <engine/client/desktop-platform-mouse-button.h>
#include <engine/client/desktop-platform-utility.h>
#include <engine/core/init.h>
#include <engine/gui/gui-key-event.h>
#include <engine/gui/gui-mouse-event.h>
#include <engine/gui/gui-scroll-event.h>
#include <engine/render/render-config.h>
#include <engine/render/rhi-device-factory.h>
#include <engine/render/rhi-device.h>
#include <utility>

// SDL3 headers use C-style casts internally; suppress warnings.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#include <SDL3/SDL.h>
#pragma clang diagnostic pop

namespace eng::client {

// The mirrored SDL button indices in desktop-platform-mouse-button.h exist so
// the mapping is testable without SDL headers. This is where they are checked
// against the real thing.
static_assert(DesktopPlatformMouseButton::LEFT == SDL_BUTTON_LEFT);
static_assert(DesktopPlatformMouseButton::MIDDLE == SDL_BUTTON_MIDDLE);
static_assert(DesktopPlatformMouseButton::RIGHT == SDL_BUTTON_RIGHT);

static_assert(static_cast<uint32_t>(SDLK_ESCAPE) ==
              DesktopPlatformKeycode::ESCAPE);
static_assert(static_cast<uint32_t>(SDLK_RETURN) ==
              DesktopPlatformKeycode::KEY_RETURN);

namespace {

  /// Fraction of the primary display to fill when auto-sizing the window.
  constexpr float DISPLAY_FILL = 0.8f;

  /// Compute window dimensions from display resolution, falling back to config.
  std::pair<int, int>
  computeWindowSize(const eng::client::GameClientConfig& config) {
    auto display = SDL_GetPrimaryDisplay();
    const auto* mode = SDL_GetCurrentDisplayMode(display);
    if (mode != nullptr) {
      auto w = static_cast<int>(static_cast<float>(mode->w) * DISPLAY_FILL);
      auto h = static_cast<int>(static_cast<float>(mode->h) * DISPLAY_FILL);
      return {w, h};
    }
    return {static_cast<int>(config.window_width),
            static_cast<int>(config.window_height)};
  }

  /// Create an SDL3 window with the given config.
  SDL_Window*
  createPlatformWindow(const eng::client::GameClientConfig& config) {
    auto [w, h] = computeWindowSize(config);
    auto* win =
        SDL_CreateWindow(config.window_title.c_str(), w, h,
                         SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (win != nullptr) {
      SDL_SetWindowMinimumSize(win, static_cast<int>(config.min_window_width),
                               static_cast<int>(config.min_window_height));
    }
    return win;
  }

  /// Build a GUI mouse event from an SDL button event.
  eng::GuiMouseEvent mouseEventFromSdl(const SDL_MouseButtonEvent& button,
                                       eng::GuiMouseEventType type) {
    eng::GuiMouseEvent me{};
    me.type = type;
    me.x = static_cast<float>(button.x);
    me.y = static_cast<float>(button.y);
    me.button = eng::client::mapDesktopMouseButton(button.button);
    me.shift_held = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
    return me;
  }

  /// Build an EngineConfig from the game client config.
  EngineConfig buildEngineConfig(const eng::client::GameClientConfig& config) {
    EngineConfig engine_config;
    engine_config.mode = EngineMode::FULL;
    engine_config.data_dir = config.data_dir;
    return engine_config;
  }

  /// Store drawable pixel size from the SDL window (HiDPI-aware).
  void queryBackbufferSize(SDL_Window* win, uint32_t& w, uint32_t& h) {
    int iw = 0;
    int ih = 0;
    if (win == nullptr || !SDL_GetWindowSizeInPixels(win, &iw, &ih)) {
      w = 1;
      h = 1;
      return;
    }
    w = static_cast<uint32_t>(std::max(iw, 1));
    h = static_cast<uint32_t>(std::max(ih, 1));
  }

  /// Create an EngineContext via the Initializer.
  std::optional<std::unique_ptr<EngineContext>>
  initEngine(const eng::client::GameClientConfig& config) {
    auto engine_config = buildEngineConfig(config);
    Initializer initializer(std::move(engine_config));
    return initializer.initAll(nullptr);
  }

  /// Shut down the engine via the Initializer.
  void shutdownEngine(std::unique_ptr<EngineContext>& engine) {
    if (engine == nullptr) {
      return;
    }
    Initializer initializer(EngineConfig{});
    initializer.shutdownAll(*engine);
    engine.reset();
  }

  /// The modifier state SDL reports alongside a keyboard event.
  DesktopGameClient::ClientKeyModifiers
  buildKeyModifiers(const SDL_KeyboardEvent& key) {
    return {.shift = (key.mod & SDL_KMOD_SHIFT) != 0,
            .ctrl = (key.mod & SDL_KMOD_CTRL) != 0,
            .alt = (key.mod & SDL_KMOD_ALT) != 0,
            .gui = (key.mod & SDL_KMOD_GUI) != 0};
  }

  /// Compute delta time since last frame in seconds.
  float
  computeDeltaTime(std::chrono::steady_clock::time_point& last_frame_time) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - last_frame_time;
    last_frame_time = now;
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
    return static_cast<float>(us.count()) / 1'000'000.0f;
  }

}  // namespace

// NOLINTNEXTLINE(bugprone-exception-escape) — shutdown is noexcept
DesktopGameClient::~DesktopGameClient() {
  // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall) — calls own impl
  shutdown();
}

void DesktopGameClient::onEvent(const SDL_Event& event) {
  if (event.type == SDL_EVENT_WINDOW_RESIZED ||
      event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
    syncPixelBackbufferFromWindow();
    return;
  }
  dispatchSdlInputToGui(event);
  if (event.type == SDL_EVENT_KEY_DOWN) {
    const auto kind = event.key.repeat != 0U
                          ? DesktopGameClient::ClientKeyDownKind::REPEAT
                          : DesktopGameClient::ClientKeyDownKind::FIRST_PRESS;
    onClientKeyDown(static_cast<uint32_t>(event.key.key), kind,
                    buildKeyModifiers(event.key));
  }
}

std::pair<int, int> DesktopGameClient::windowClientSizePx() const {
  int w = 0;
  int h = 0;
  if (window_ != nullptr) {
    SDL_GetWindowSize(window_, &w, &h);
  }
  return {w, h};
}

void DesktopGameClient::setWindowTitle(std::string_view utf8_title) {
  if (window_ == nullptr) {
    return;
  }
  // SDL requires a null-terminated C string; string_view is not
  // guaranteed to be terminated, so copy into an owning std::string.
  std::string title{utf8_title};
  SDL_SetWindowTitle(window_, title.c_str());
}

uint32_t DesktopGameClient::guiLayoutWidth() const {
  const auto wh = windowClientSizePx();
  return static_cast<uint32_t>(std::max(wh.first, 1));
}

uint32_t DesktopGameClient::guiLayoutHeight() const {
  const auto wh = windowClientSizePx();
  return static_cast<uint32_t>(std::max(wh.second, 1));
}

float DesktopGameClient::textRasterSupersample() const {
  if (window_ == nullptr) {
    return 1.0f;
  }
  const float d = SDL_GetWindowPixelDensity(window_);
  return (d > 0.0f) ? d : 1.0f;
}

void DesktopGameClient::applyBackbufferResize(uint32_t w, uint32_t h) {
  backbuffer_width_ = w;
  backbuffer_height_ = h;
  if (rhi_device_ != nullptr) {
    rhi_device_->resizeSwapchain(w, h);
  }
  resizeGuiToBackbuffer();
}

void DesktopGameClient::syncPixelBackbufferFromWindow() {
  uint32_t nw = 0;
  uint32_t nh = 0;
  queryBackbufferSize(window_, nw, nh);
  if (nw != backbuffer_width_ || nh != backbuffer_height_) {
    applyBackbufferResize(nw, nh);
  }
}

void DesktopGameClient::syncSdlTextInputState() {
  bool need = guiWidgetTree().hasFocusedInput();
  bool active = SDL_TextInputActive(window_);
  if (need && !active) {
    SDL_StartTextInput(window_);
  } else if (!need && active) {
    SDL_StopTextInput(window_);
  }
}

namespace {

  /// Build a GuiKeyEvent from an SDL3 keyboard event.
  eng::GuiKeyEvent buildKeyEvent(const SDL_KeyboardEvent& key) {
    eng::GuiKeyEvent e{};
    e.keycode = static_cast<uint32_t>(key.key);
    e.scancode = static_cast<uint32_t>(key.scancode);
    e.pressed = true;
    e.repeat = key.repeat;
    const auto mods = buildKeyModifiers(key);
    e.shift = mods.shift;
    e.ctrl = mods.ctrl;
    e.alt = mods.alt;
    e.gui = mods.gui;
    return e;
  }

}  // namespace

// Named algorithm: SDL3 event type -> engine GUI dispatch (desktop-only).
// All dispatch branches are trivial one-liner mappings.
void DesktopGameClient::
    dispatchSdlInputToGui(  // NOLINT(readability-function-size)
                            // — named algorithm
        const SDL_Event& event) {
  switch (event.type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
      guiDispatchMouseDown(
          mouseEventFromSdl(event.button, eng::GuiMouseEventType::BUTTON_DOWN));
      syncSdlTextInputState();
      break;
    }
    case SDL_EVENT_MOUSE_BUTTON_UP: {
      guiDispatchMouseUp(
          mouseEventFromSdl(event.button, eng::GuiMouseEventType::BUTTON_UP));
      syncSdlTextInputState();
      break;
    }
    case SDL_EVENT_MOUSE_MOTION: {
      eng::GuiMouseEvent me;
      me.type = eng::GuiMouseEventType::MOVE;
      me.x = static_cast<float>(event.motion.x);
      me.y = static_cast<float>(event.motion.y);
      me.shift_held = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
      guiDispatchMouseMove(me);
      break;
    }
    case SDL_EVENT_MOUSE_WHEEL: {
      eng::GuiScrollEvent se;
      se.x = static_cast<float>(event.wheel.mouse_x);
      se.y = static_cast<float>(event.wheel.mouse_y);
      se.delta_y = static_cast<float>(event.wheel.y);
      guiDispatchScroll(se);
      break;
    }
    case SDL_EVENT_TEXT_INPUT: {
      const char* utf8 = event.text.text;
      if (utf8 != nullptr) {
        (void)guiDispatchText(utf8);
      }
      break;
    }
    case SDL_EVENT_KEY_DOWN:
      (void)guiDispatchKey(buildKeyEvent(event.key));
      break;
    default:
      break;
  }
}

bool DesktopGameClient::pollEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_EVENT_QUIT) {
      return false;
    }
    onEvent(event);
  }
  return true;
}

std::optional<std::string>
DesktopGameClient::initSdlWindow(const eng::client::GameClientConfig& config) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    return "Failed to initialize SDL3";
  }
  window_ = createPlatformWindow(config);
  if (window_ == nullptr) {
    return "Failed to create SDL3 window";
  }
  queryBackbufferSize(window_, backbuffer_width_, backbuffer_height_);
  return std::nullopt;
}

std::optional<std::string> DesktopGameClient::initRhiDevice() {
  eng::RenderConfig render_cfg{};
  render_cfg.native_window = window_;
  render_cfg.backbuffer_width = backbuffer_width_;
  render_cfg.backbuffer_height = backbuffer_height_;
  auto rhi = eng::render::RhiDeviceFactory::create(render_cfg);
  if (!rhi.has_value()) {
    return "RHI device creation failed";
  }
  rhi_device_ = std::move(*rhi);
  return std::nullopt;
}

std::optional<std::string>
DesktopGameClient::initEngineContext(const eng::client::GameClientConfig& cfg) {
  auto engine = initEngine(cfg);
  if (!engine.has_value()) {
    return "Engine initialization failed";
  }
  engine_ = std::move(*engine);
  return std::nullopt;
}

std::optional<std::string>
DesktopGameClient::init(const eng::client::GameClientConfig& config) {
  if (initialized_) {
    return "Game client already initialized";
  }
  DesktopPlatformUtility::install();
  if (auto err = initSdlWindow(config)) {
    return err;
  }
  if (auto err = initEngineContext(config)) {
    return err;
  }
  if (auto err = initRhiDevice()) {
    return err;
  }
  initialized_ = true;
  return std::nullopt;
}

bool DesktopGameClient::runOneFrame(
    std::chrono::steady_clock::time_point& last_frame) {
  if (!pollEvents()) {
    return false;
  }
  // Between polling and ticking: the dialog callback may have run on
  // another thread, and this is where its answer joins the main thread.
  drainDialogPath();
  float dt = computeDeltaTime(last_frame);
  if (!onTick(dt)) {
    return false;
  }
  presentGuiFrame();
  return true;
}

namespace {

  /// SDL hands the callback a null list on error and an empty one on
  /// cancel; only a first entry is a real answer.
  const char* firstChosenPath(const char* const* filelist) {
    if (filelist == nullptr || filelist[0] == nullptr) {
      return nullptr;
    }
    return filelist[0];
  }

}  // namespace

void DesktopGameClient::storeDialogPath(DialogPurpose purpose,
                                        const char* path) {
  const std::lock_guard<std::mutex> lock(dialog_mutex_);
  pending_dialog_path_ = std::make_pair(purpose, std::string(path));
}

void DesktopGameClient::drainDialogPath() {
  std::optional<std::pair<DialogPurpose, std::string>> chosen;
  {
    const std::lock_guard<std::mutex> lock(dialog_mutex_);
    chosen.swap(pending_dialog_path_);
  }
  if (!chosen.has_value()) {
    return;
  }
  const std::filesystem::path path(chosen->second);
  if (chosen->first == DialogPurpose::SAVE_LOCATION) {
    onSaveLocationChosen(path);
    return;
  }
  onFolderChosen(path);
}

namespace {

  /// Where both dialogs open. Empty means "no default location", which is
  /// what SDL takes a null for.
  std::string dialogStartFolder() {
    return chooseDialogStartFolder(SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS),
                                   SDL_GetUserFolder(SDL_FOLDER_HOME));
  }

  /// Null when the folder is unknown, which SDL reads as "your choice".
  const char* startFolderOrNull(const std::string& start) {
    return start.empty() ? nullptr : start.c_str();
  }

}  // namespace

void DesktopGameClient::showSaveLocationDialog() {
  const std::string start = dialogStartFolder();
  SDL_ShowSaveFileDialog(
      [](void* userdata, const char* const* filelist, int /*filter*/) {
        const char* path = firstChosenPath(filelist);
        if (path != nullptr) {
          static_cast<DesktopGameClient*>(userdata)->storeDialogPath(
              DialogPurpose::SAVE_LOCATION, path);
        }
      },
      this, window_, nullptr, 0, startFolderOrNull(start));
}

void DesktopGameClient::showOpenFolderDialog() {
  const std::string start = dialogStartFolder();
  SDL_ShowOpenFolderDialog(
      [](void* userdata, const char* const* filelist, int /*filter*/) {
        const char* path = firstChosenPath(filelist);
        if (path != nullptr) {
          static_cast<DesktopGameClient*>(userdata)->storeDialogPath(
              DialogPurpose::OPEN_FOLDER, path);
        }
      },
      this, window_, startFolderOrNull(start), false);
}

void DesktopGameClient::run() {
  if (!initialized_ || engine_ == nullptr) {
    return;
  }
  if (!onInit()) {
    return;
  }
  running_ = true;
  auto last_frame = std::chrono::steady_clock::now();
  while (running_ && runOneFrame(last_frame)) {
  }
  running_ = false;
}

void DesktopGameClient::shutdown() {
  if (!initialized_) {
    return;
  }
  onShutdown();
  rhi_device_.reset();
  shutdownEngine(engine_);
  if (window_ != nullptr) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
  SDL_Quit();
  initialized_ = false;
}

}  // namespace eng::client
