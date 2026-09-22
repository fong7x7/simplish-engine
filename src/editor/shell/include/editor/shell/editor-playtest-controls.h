#pragma once

/// @file editor-playtest-controls.h
/// @brief How keys and pads steer a playtest: the default control scheme,
///        the pad's way through the character selector, and which way the
///        camera says "up" is.
/// @par Threading Thread-safe (pure functions).

#include <cstdint>
#include <editor/shell/iso-axes.h>
#include <engine/input/gamepad-button.h>
#include <engine/input/gamepad-family.h>
#include <engine/input/input-bindings.h>
#include <engine/input/move-basis.h>
#include <optional>

namespace eng::editor {

/// The control scheme a playtest starts from before the user's own file
/// is read: WASD and the arrow keys move, and every pad plays twin-stick —
/// the left stick and d-pad move, the right stick aims, the right trigger
/// or shoulder fires. Fire is also the viewport's left button, which is
/// not a binding: the viewport owns its clicks.
[[nodiscard]] input::InputBindings editorDefaultInputBindings();

/// The key @p button, on a @p family pad, stands for in the character
/// selector — the d-pad steps through the cards, and the face buttons pick
/// one and back out as every menu does on that pad (`gui-nav-buttons.h`),
/// so A confirms on a Nintendo pad — or nothing.
[[nodiscard]] std::optional<uint32_t>
editorChoosingKeyFor(input::GamepadButton button, input::GamepadFamily family);

/// The world directions the screen's right and down point along under the
/// projection @p axes, for camera-relative movement: W moves the player up
/// the screen, which under the isometric view is a diagonal across the grid.
///
/// Derived from the axes rather than written per projection, so a projection
/// added to `iso-axes.h` steers correctly without a case here. The two
/// directions are normalised; both projections are rotations of the ground,
/// so they come out perpendicular.
[[nodiscard]] input::MoveBasis editorMoveBasis(const IsoAxes& axes);

}  // namespace eng::editor
