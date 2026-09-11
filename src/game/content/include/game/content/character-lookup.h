#pragma once

/// @file character-lookup.h
/// @brief Finding a character by id, and what its stats come to per tick.
/// @par Threading
/// Pure functions over value types.

#include <game/content/character-definition.h>
#include <game/content/game-content.h>
#include <string_view>

namespace eng::game {

/// The character nobody picked: no model, and the default stats. What a
/// player plays as when their pick is empty, or names a character the
/// content does not have.
[[nodiscard]] const CharacterDefinition& defaultCharacter();

/// The character @p id names in @p content, or `defaultCharacter()` when it
/// is empty or names none of them.
///
/// Never a failure: a setup naming a character the content lacks is still a
/// setup the game can run, and running it as the default is deterministic —
/// every peer with the same content resolves it the same way.
[[nodiscard]] const CharacterDefinition&
resolveCharacter(const GameContent& content, std::string_view id);

/// How far @p character moves in one tick at full stick, in tiles.
///
/// One division, so the default character moves exactly what the old
/// constant `5.0F / 60.0F` moved.
[[nodiscard]] float characterSpeedPerTick(const CharacterDefinition& character);

}  // namespace eng::game
