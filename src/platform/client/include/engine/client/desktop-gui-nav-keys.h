#pragma once

/// @file desktop-gui-nav-keys.h
/// @brief Which desktop keys navigate a menu, and how.
/// @par Threading
/// Thread-safe (pure function).

#include <engine/client/desktop-nav-key.h>
#include <engine/gui/gui-nav-command.h>
#include <optional>

namespace eng::client {

/// The menu command @p press asks for, or nothing: the arrows move, Tab
/// and Shift+Tab step through focus order, Enter and Space confirm, and
/// Escape cancels. While a text field is typing only Escape and Tab
/// navigate, so the arrows still move its cursor. Held arrows and Tab
/// repeat with the OS; a repeated Enter, Space or Escape is nothing, so a
/// key held down does not confirm a row, then the next dialog's.
[[nodiscard]] std::optional<GuiNavCommand>
desktopGuiNavCommand(const DesktopNavKey& press);

}  // namespace eng::client
