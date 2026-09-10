#pragma once

/// @file editor-playtest-controls.h
/// @brief How the keyboard steers a playtest: which key is which action,
///        and which way the camera says "up" is.
/// @par Threading Thread-safe (pure functions).

#include <cstdint>
#include <editor/shell/iso-axes.h>
#include <engine/input/input-action.h>
#include <engine/input/move-basis.h>
#include <optional>

namespace eng::editor {

/// The action @p key holds in a playtest — WASD and the arrow keys move —
/// or nothing. Fire is the viewport's left button, not a key.
[[nodiscard]] std::optional<input::InputAction>
editorPlaytestAction(uint32_t key);

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
