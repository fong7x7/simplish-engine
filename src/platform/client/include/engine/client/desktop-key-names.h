#pragma once

/// @file desktop-key-names.h
/// @brief The words a binding file uses for the desktop's unprintable keys.
/// @par Threading
/// Thread-safe (constant data).

#include <engine/input/key-name.h>
#include <span>

namespace eng::client {

/// The desktop's keys that have no character of their own — the arrows,
/// Space, Shift, the function keys — by the word a binding file names them
/// with: `"key:up"`, `"key:space"`, `"key:lshift"`. Letters, digits and
/// punctuation need no entry; a file names them by their character.
[[nodiscard]] std::span<const eng::input::KeyName> desktopKeyNames();

}  // namespace eng::client
