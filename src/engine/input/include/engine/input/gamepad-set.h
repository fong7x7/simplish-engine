#pragma once

/// @file gamepad-set.h
/// @brief Every connected pad, which one is in use, and what was just
///        pressed on it.
/// @par Threading
/// Main-thread-only.

#include <cstdint>
#include <engine/input/gamepad-reading.h>
#include <optional>
#include <span>
#include <vector>

namespace eng::input {

/// The pads connected this frame, as a platform backend reports them, and
/// the bookkeeping every backend would otherwise repeat: which pad the
/// player is using, and which of its buttons went down since last frame.
///
/// The pad in use is the last one touched — a button pressed or a stick or
/// trigger pushed past halfway — so a second pad lying on the desk, or one
/// that drifts, does not steal the player from the one in their hands.
class GamepadSet {
public:
  /// Replace the connected pads with @p readings, one per pad. A pad not
  /// in them has been disconnected.
  void update(std::span<const GamepadReading> readings);

  /// How many pads are connected.
  [[nodiscard]] std::size_t size() const { return pads_.size(); }

  /// The pad the player is using, or null with none connected.
  [[nodiscard]] const GamepadState* active() const;

  /// Whose layout the pad in use follows; GENERIC with none connected.
  [[nodiscard]] GamepadFamily activeFamily() const;

  /// Whether @p button went down on the pad in use since the last update —
  /// for menus, which act on presses rather than on what is held.
  [[nodiscard]] bool pressed(GamepadButton button) const;

private:
  /// One connected pad, now and a frame ago.
  struct Pad {
    /// The backend's id for it.
    uint64_t device = 0;
    /// What it reads this frame.
    GamepadState now;
    /// What it read last frame; all released when it has just connected.
    GamepadState before;
    /// Whose layout it follows.
    GamepadFamily family = GamepadFamily::GENERIC;
  };

  /// Make @p touched_device the pad in use when there is one; otherwise
  /// keep the one in use while it is connected, or fall back to the first.
  void chooseActive(std::optional<uint64_t> touched_device);

  /// The pad @p device is, or null when it is not connected.
  [[nodiscard]] const Pad* find(uint64_t device) const;

  /// The pad in use, or null.
  [[nodiscard]] const Pad* activePad() const;

  /// Each connected pad, in the order the backend reported them.
  std::vector<Pad> pads_{};
  /// The id of the pad in use, if any is.
  std::optional<uint64_t> active_{};
};

}  // namespace eng::input
