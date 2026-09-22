#pragma once

/// @file input-bindings-json.h
/// @brief Control schemes to and from the JSON a player can edit.
/// @par Threading
/// Pure functions.

#include <engine/input/input-bindings-load.h>
#include <engine/input/input-bindings.h>
#include <engine/input/key-name.h>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace eng::input {

/// @p action's name in a bindings file: "move_up", "fire", "aim_left".
[[nodiscard]] std::string_view inputActionName(InputAction action);

/// The action named @p name, or nothing.
[[nodiscard]] std::optional<InputAction>
inputActionNamed(std::string_view name);

/// @p source as a bindings file writes it — "key:w", "pad:south",
/// "pad:-left_y" — naming keys from @p keys.
[[nodiscard]] std::string inputSourceText(InputSource source,
                                          std::span<const KeyName> keys);

/// The control @p text names, in that notation, or nothing.
[[nodiscard]] std::optional<InputSource>
parseInputSource(std::string_view text, std::span<const KeyName> keys);

/// @p bindings as JSON: a `deadzones` object and an `actions` object
/// listing each action's controls as strings — `"key:w"`, `"key:up"`,
/// `"pad:south"`, `"pad:-left_y"`, `"pad:right_trigger"`. Keys are named
/// from @p keys where they appear there, as their character when they are
/// printable, and as a hexadecimal symbol otherwise.
[[nodiscard]] std::string writeInputBindings(const InputBindings& bindings,
                                             std::span<const KeyName> keys);

/// The scheme @p json describes, over @p defaults: an action the file
/// lists gets exactly the controls listed, and one it leaves out keeps
/// its default, so an action added after the file was written still has
/// a binding. Anything unreadable is skipped and reported; text that is
/// not JSON at all gives the defaults and one problem.
[[nodiscard]] InputBindingsLoad
parseInputBindings(std::string_view json, const InputBindings& defaults,
                   std::span<const KeyName> keys);

}  // namespace eng::input
