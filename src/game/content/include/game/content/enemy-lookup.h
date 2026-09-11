#pragma once

/// @file enemy-lookup.h
/// @brief Finding an enemy archetype by id.
/// @par Threading
/// Pure functions over value types.

#include <game/content/enemy-definition.h>
#include <game/content/game-content.h>
#include <string_view>

namespace eng::game {

/// The archetype @p id names in @p content, or null when none does.
///
/// A failure the caller sees, unlike `resolveCharacter`: a director asked
/// to spawn an archetype the content lacks has nothing sensible to spawn,
/// and spawning nothing is itself deterministic.
[[nodiscard]] const EnemyDefinition* findEnemy(const GameContent& content,
                                               std::string_view id);

}  // namespace eng::game
