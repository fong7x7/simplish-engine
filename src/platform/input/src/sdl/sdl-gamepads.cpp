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
  /// SDL's full-strength motor setting.
  constexpr float SDL_MOTOR_MAX = 65535.0F;

  /// @p strength, 0 to 1, as an SDL motor setting.
  Uint16 motor(float strength) {
    return static_cast<Uint16>(std::clamp(strength, 0.0F, 1.0F) *
                               SDL_MOTOR_MAX);
  }

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

  /// One SDL pad type and the family whose layout it follows.
  struct TypeFamily {
    /// SDL's type.
    SDL_GamepadType type;
    /// The engine's family for it.
    GamepadFamily family;
  };

  /// Every SDL type that is not a generic pad.
  constexpr TypeFamily TYPE_FAMILIES[] = {
      {SDL_GAMEPAD_TYPE_XBOX360, GamepadFamily::XBOX},
      {SDL_GAMEPAD_TYPE_XBOXONE, GamepadFamily::XBOX},
      {SDL_GAMEPAD_TYPE_PS3, GamepadFamily::PLAYSTATION},
      {SDL_GAMEPAD_TYPE_PS4, GamepadFamily::PLAYSTATION},
      {SDL_GAMEPAD_TYPE_PS5, GamepadFamily::PLAYSTATION},
      {SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO, GamepadFamily::NINTENDO},
      {SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT, GamepadFamily::NINTENDO},
      {SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT, GamepadFamily::NINTENDO},
      {SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR, GamepadFamily::NINTENDO},
  };

  /// Whose layout SDL says @p pad follows.
  GamepadFamily familyOf(SDL_Gamepad* pad) {
    const SDL_GamepadType type = SDL_GetGamepadType(pad);
    for (const TypeFamily& entry : TYPE_FAMILIES) {
      if (entry.type == type) {
        return entry.family;
      }
    }
    return GamepadFamily::GENERIC;
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
        const GamepadState state =
            focus == WindowFocus::FOCUSED ? readPad(pad) : GamepadState{};
        readings.push_back({ids[i], state, familyOf(pad)});
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

bool Gamepads::rumble(const GamepadRumble& rumble) {
  const std::optional<uint64_t> device = pads_.activeDevice();
  return device && this->rumble(*device, rumble);
}

bool Gamepads::rumble(uint64_t device, const GamepadRumble& rumble) {
  SDL_Gamepad* pad =
      opened_ ? SDL_GetGamepadFromID(static_cast<SDL_JoystickID>(device))
              : nullptr;
  if (pad == nullptr || focus_ != WindowFocus::FOCUSED) {
    return false;
  }
  const auto ms = static_cast<Uint32>(std::max(0.0F, rumble.seconds) * 1000.0F);
  // Trigger motors are an Xbox pad's; anywhere else this simply fails.
  (void)SDL_RumbleGamepadTriggers(pad, motor(rumble.left_trigger),
                                  motor(rumble.right_trigger), ms);
  return SDL_RumbleGamepad(pad, motor(rumble.low), motor(rumble.high), ms);
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
