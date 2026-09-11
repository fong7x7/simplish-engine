#pragma once

/// @file character-definition.h
/// @brief One character a player can play as: what it is called, what it
/// looks like, and the numbers it plays by.
/// @par Threading
/// A value type.

#include <cstdint>
#include <string>

namespace eng::game {

/// How fast the default character moves, in tiles a second: the speed every
/// player had before characters existed.
inline constexpr float DEFAULT_CHARACTER_MOVE_SPEED = 5.0F;

/// How many health segments the default character has (Game REQUIREMENTS
/// §3.3: discrete segments rather than a bar).
inline constexpr uint16_t DEFAULT_CHARACTER_HEALTH = 5;

/// A character, as a data table defines one (project-format §8).
///
/// The simulation reads the stats and nothing else; `name` is for the
/// selector and `model` for the renderer. They live in one record anyway
/// because a character is authored as one thing, and ADR-007 wants one
/// in-memory representation whether the JSON or the generated tables
/// filled it.
///
/// A loadout — the weapons and consumables a character starts with — joins
/// this record when weapons exist to put in it.
struct CharacterDefinition {
  /// Stable identifier, as a setup and a replay name it: `scout`.
  std::string id{};
  /// What the selector calls it: `Scout`.
  std::string name{};
  /// The asset it is drawn as — `mesh:characters_scout` — or empty for the
  /// stand-in. Presentation: the simulation never reads it.
  std::string model{};
  /// How fast it moves at full stick, in tiles a second.
  float move_speed = DEFAULT_CHARACTER_MOVE_SPEED;
  /// How many health segments it starts with.
  uint16_t health = DEFAULT_CHARACTER_HEALTH;
};

}  // namespace eng::game
