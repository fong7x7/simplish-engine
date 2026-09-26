#pragma once

/// @file player-input.h
/// @brief What a player is doing with their controls this tick.
/// @par Threading
/// Pure reads of the world; call inside the logic's tick.

#include <game/logic/game-logic-world.h>
#include <game/logic/logic-player.h>

namespace eng::game::sdk {

/// Whether @p player is holding fire this tick: the trigger, the left
/// mouse button, or `send_input`'s `fire`. A player who is not up fires
/// nothing, and nobody fires while the game is paused, so this is false
/// then. Aim is `LogicPlayer::aim`.
[[nodiscard]] bool firing(const GameLogicWorld& world,
                          const LogicPlayer& player);

}  // namespace eng::game::sdk
