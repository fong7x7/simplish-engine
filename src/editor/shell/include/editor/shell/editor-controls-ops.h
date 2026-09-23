#pragma once

/// @file editor-controls-ops.h
/// @brief What rebinding one action does to a control scheme, and what the
///        Controls screen calls each action and control.
/// @par Threading Thread-safe (pure functions).

#include <engine/input/gamepad-family.h>
#include <engine/input/input-action.h>
#include <engine/input/input-bindings.h>
#include <engine/input/input-source.h>
#include <string>
#include <string_view>

namespace eng::editor {

/// Whether @p source is on a pad rather than the keyboard.
[[nodiscard]] bool editorIsPadControl(input::InputSource source);

/// Bind @p source to @p action as the Controls screen does: it replaces
/// the action's controls on the same device — a key replaces its keys, a
/// pad control its pad controls — and leaves the other device's alone, so
/// rebinding a key never loses the stick.
void editorRebind(input::InputBindings& bindings, input::InputAction action,
                  input::InputSource source);

/// What the Controls screen calls @p action: "Move up", "Fire".
[[nodiscard]] std::string_view editorActionLabel(input::InputAction action);

/// What the Controls screen calls @p source on a @p family pad: "W", "Up",
/// "Space", "Cross", "RT", "Left Stick Up".
[[nodiscard]] std::string editorControlLabel(input::InputSource source,
                                             input::GamepadFamily family);

/// Every pad control bound to @p action, labelled as a @p pad_family pad
/// prints them and joined with " / ", or "—" when there are none.
[[nodiscard]] std::string
editorActionControlsLabel(const input::InputBindings& bindings,
                          input::InputAction action,
                          input::GamepadFamily pad_family);

/// Every key bound to @p action, labelled and joined the same way.
[[nodiscard]] std::string
editorActionKeysLabel(const input::InputBindings& bindings,
                      input::InputAction action);

}  // namespace eng::editor
