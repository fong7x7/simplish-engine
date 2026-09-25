#pragma once

/// @file ui.h
/// @brief Driving the game's own screens from its logic.
/// @par Threading
/// Main-thread-only; the logic's own thread, inside its tick.

#include <cstdint>
#include <game/logic/game-logic-world.h>
#include <game/logic/logic-event.h>
#include <string_view>

namespace eng::game::sdk {

/// Set the value @p key the screens show to the number @p value.
void setUiNumber(GameLogicWorld& world, std::string_view key, int64_t value);

/// Whether @p event is a player choosing @p action on a screen.
[[nodiscard]] bool chose(const LogicEvent& event, std::string_view action);

}  // namespace eng::game::sdk
