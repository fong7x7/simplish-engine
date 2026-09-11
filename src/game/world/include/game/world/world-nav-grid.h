#pragma once

/// @file world-nav-grid.h
/// @brief The navigation grid a run of a setup plans across.
/// @par Threading
/// Pure functions.

#include <engine/spatial/nav-grid.h>
#include <game/world/game-setup.h>

namespace eng::game {

/// The navigation grid for @p setup: its obstacles, over a rectangle taking
/// in every player's and every actor's spawn with a margin to spare, on the
/// floor the first player stands on.
///
/// What a `GameWorld` plans across when it has actors to plan for, and what
/// the editor shows and checks a level against — one function, so the
/// overlay and the game cannot disagree about where a corridor is.
[[nodiscard]] spatial::NavGrid buildWorldNavGrid(const GameSetup& setup);

}  // namespace eng::game
