#pragma once

/// @file gamepads.h
/// @brief The pads this platform supports, read once a frame.
/// @par Threading
/// Main-thread-only.

#include <cstdint>
#include <engine/input/gamepad-rumble.h>
#include <engine/input/gamepad-set.h>
#include <engine/input/window-focus.h>
#include <optional>
#include <string>
#include <vector>

namespace eng::input {

/// The platform's pad backend: finds the pads this target supports, opens
/// them as they are plugged in, and reads them into a `GamepadSet` in the
/// engine's canonical buttons and axes.
///
/// Which pads exist is the platform's decision, made when it is built:
/// exactly one backend is compiled in (`src/platform/input/CMakeLists.txt`).
/// The desktop's is SDL3's, which takes every pad SDL knows — Xbox,
/// DualShock 4, DualSense, Switch Pro, Joy-Cons alone or paired, and any
/// generic pad with a mapping. A console's reads only that console's own
/// pads, and a target with none compiled in reports no pads at all. Nothing
/// above this layer knows which it got.
class Gamepads {
public:
  Gamepads() = default;
  ~Gamepads();
  Gamepads(const Gamepads&) = delete;
  Gamepads& operator=(const Gamepads&) = delete;
  Gamepads(Gamepads&&) = delete;
  Gamepads& operator=(Gamepads&&) = delete;

  /// Start the backend. A reason on failure, after which there are simply
  /// no pads — a game is playable without one, so this is never fatal.
  std::optional<std::string> open();

  /// Close every pad and stop the backend. Safe to call twice.
  void close();

  /// Read every connected pad into `pads()`, opening new ones and dropping
  /// ones unplugged. Once a frame, after the platform's events are pumped.
  void poll();

  /// Play @p rumble on the pad in use, replacing whatever it was playing,
  /// on the motors it has. False when there is no pad, it has no motors,
  /// the window is unfocused, or the backend has no rumble at all.
  bool rumble(const GamepadRumble& rumble);

  /// Play @p rumble on pad @p device, as `rumble` does on the pad in use —
  /// for a couch of players, each feeling their own.
  bool rumble(uint64_t device, const GamepadRumble& rumble);

  /// Whether the window has focus; while it does not, pads read as resting.
  void setFocus(WindowFocus focus) { focus_ = focus; }

  /// Every pad as of the last `poll`.
  [[nodiscard]] const GamepadSet& pads() const { return pads_; }

private:
  /// The backend's ids for the pads it has open.
  std::vector<uint64_t> open_devices_{};
  /// The pads as last read.
  GamepadSet pads_{};
  /// Whether the window has focus.
  WindowFocus focus_ = WindowFocus::FOCUSED;
  /// Whether `open` succeeded and `close` has not run since.
  bool opened_ = false;
};

}  // namespace eng::input
