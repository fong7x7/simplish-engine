#pragma once

/// @file game-content.h
/// @brief Every table the game's content defines.
/// @par Threading
/// A value type; read-only once a run starts.

#include <game/content/character-definition.h>
#include <vector>

namespace eng::game {

/// The content a run is played with (ADR-007): filled from a project's JSON
/// by the editor today, and from generated tables in a shipping build.
///
/// Separate from a `GameSetup`, which says which of these each player picked
/// and where they start: the same content serves every run, and a setup is
/// one run's choices within it.
struct GameContent {
  /// Every character a player can pick, in the order the table lists them.
  std::vector<CharacterDefinition> characters{};
};

}  // namespace eng::game
