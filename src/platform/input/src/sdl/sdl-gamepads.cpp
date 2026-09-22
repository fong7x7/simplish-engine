// The desktop's pad backend: SDL3's gamepad API, which knows Xbox,
// PlayStation and Nintendo pads — Joy-Cons included, paired or alone — and
// any generic pad with a community mapping, and reports them all in the
// same positional layout the engine's GamepadButton names.

#include <SDL3/SDL.h>
#include <algorithm>
#include <engine/input/gamepads.h>

namespace eng::input {

// The engine's buttons and axes are SDL's, in SDL's order, so a cast is
// the whole mapping. These fail the build if either side reorders.
static_assert(static_cast<int>(GamepadButton::SOUTH) ==
              SDL_GAMEPAD_BUTTON_SOUTH);
static_assert(static_cast<int>(GamepadButton::START) ==
              SDL_GAMEPAD_BUTTON_START);
static_assert(static_cast<int>(GamepadButton::DPAD_RIGHT) ==
              SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
static_assert(static_cast<int>(GamepadButton::MISC) ==
              SDL_GAMEPAD_BUTTON_MISC1);
static_assert(static_cast<int>(GamepadButton::LEFT_PADDLE_2) ==
              SDL_GAMEPAD_BUTTON_LEFT_PADDLE2);
static_assert(static_cast<int>(GamepadButton::TOUCHPAD) ==
              SDL_GAMEPAD_BUTTON_TOUCHPAD);
static_assert(static_cast<int>(GamepadAxis::LEFT_X) == SDL_GAMEPAD_AXIS_LEFTX);
static_assert(static_cast<int>(GamepadAxis::RIGHT_Y) ==
              SDL_GAMEPAD_AXIS_RIGHTY);
static_assert(static_cast<int>(GamepadAxis::RIGHT_TRIGGER) ==
              SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
static_assert(GAMEPAD_AXIS_COUNT == SDL_GAMEPAD_AXIS_COUNT);

namespace {

  /// SDL's full-scale axis reading.
  constexpr float SDL_AXIS_MAX = 32767.0F;

  /// What @p pad's controls read now, in the engine's terms.
  GamepadState readPad(SDL_Gamepad* pad) {
    GamepadState state;
    for (std::size_t i = 0; i < GAMEPAD_BUTTON_COUNT; ++i) {
      if (SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(i))) {
        state.press(static_cast<GamepadButton>(i));
      }
    }
    for (std::size_t i = 0; i < GAMEPAD_AXIS_COUNT; ++i) {
      const Sint16 raw =
          SDL_GetGamepadAxis(pad, static_cast<SDL_GamepadAxis>(i));
      state.setAxis(static_cast<GamepadAxis>(i),
                    static_cast<float>(raw) / SDL_AXIS_MAX);
    }
    return state;
  }

  /// The pad SDL knows as @p id, opening it if nothing has yet.
  SDL_Gamepad* openPad(SDL_JoystickID id) {
    SDL_Gamepad* pad = SDL_GetGamepadFromID(id);
    return pad != nullptr ? pad : SDL_OpenGamepad(id);
  }

  /// Close the pad SDL knows as @p id, if it is open.
  void closePad(uint64_t id) {
    if (SDL_Gamepad* pad =
            SDL_GetGamepadFromID(static_cast<SDL_JoystickID>(id))) {
      SDL_CloseGamepad(pad);
    }
  }

  /// Every pad SDL has connected, opened and read — or read as resting
  /// while the window is @p focus UNFOCUSED.
  std::vector<GamepadReading> readConnected(WindowFocus focus) {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    std::vector<GamepadReading> readings;
    for (int i = 0; ids != nullptr && i < count; ++i) {
      if (SDL_Gamepad* pad = openPad(ids[i])) {
        readings.push_back({ids[i], focus == WindowFocus::FOCUSED
                                        ? readPad(pad)
                                        : GamepadState{}});
      }
    }
    SDL_free(ids);
    return readings;
  }

  /// Close every pad in @p open that is not among @p connected, and leave
  /// @p open holding exactly the connected ones.
  void retireUnplugged(std::vector<uint64_t>& open,
                       const std::vector<GamepadReading>& connected) {
    for (const uint64_t id : open) {
      if (std::ranges::find(connected, id, &GamepadReading::device) ==
          connected.end()) {
        closePad(id);
      }
    }
    open.clear();
    for (const GamepadReading& reading : connected) {
      open.push_back(reading.device);
    }
  }

}  // namespace

Gamepads::~Gamepads() {
  close();
}

std::optional<std::string> Gamepads::open() {
  if (opened_) {
    return std::nullopt;
  }
  if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
    return std::string{"SDL gamepad subsystem: "} + SDL_GetError();
  }
  opened_ = true;
  return std::nullopt;
}

void Gamepads::close() {
  if (!opened_) {
    return;
  }
  std::ranges::for_each(open_devices_, closePad);
  open_devices_.clear();
  pads_.update({});
  SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
  opened_ = false;
}

void Gamepads::poll() {
  if (!opened_) {
    return;
  }
  const std::vector<GamepadReading> readings = readConnected(focus_);
  retireUnplugged(open_devices_, readings);
  pads_.update(readings);
}

}  // namespace eng::input
