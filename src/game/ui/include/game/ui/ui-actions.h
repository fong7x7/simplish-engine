#pragma once

/// @file ui-actions.h
/// @brief Every action a game's screens name.
/// @par Threading
/// Pure.

#include <game/ui/ui-screen.h>
#include <span>
#include <string>
#include <vector>

namespace eng::game {

/// Every action any of @p screens' buttons names, sorted and once each:
/// the project's action list, which `PlayerInput::ui_action` numbers by
/// (ADR-012). Sorted, so every peer and every replay number them alike.
[[nodiscard]] std::vector<std::string>
uiActions(std::span<const UiScreen> screens);

}  // namespace eng::game
