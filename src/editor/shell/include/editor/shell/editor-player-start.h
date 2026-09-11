#pragma once

/// @file editor-player-start.h
/// @brief Where one player enters the level.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <editor/shell/iso-projection.h>
#include <string>

namespace eng::editor {

/// A point a player spawns at when the level starts.
///
/// Each carries the player it is for, 1 to `EDITOR_PLAYER_SLOTS`, because a
/// session holds up to four players ([Game REQUIREMENTS §8]) and each needs
/// somewhere of their own to stand. More than one start may name the same
/// player; which of them the game uses is the game's decision, not the
/// format's. Saved with the level as an entity of definition
/// `entity:player_start` ([project-format.md §4]).
///
/// No facing: the camera never rotates and players aim freely, so the
/// direction somebody spawns looking in is the direction they are already
/// pointing the stick.
/// @thread_safety Main-thread-only.
struct EditorPlayerStart {
  /// Stable identifier for this one start: `start_01`. Assigned when it is
  /// added and never reused, so a logic file can say
  /// `player_start:start_01` — see `editor-entity-id.h`.
  std::string id{};
  /// Which player spawns here, 1 to `EDITOR_PLAYER_SLOTS`.
  uint8_t player = 1;
  /// Where the player's feet land: the centre of the tile it was dropped
  /// on, on the ground.
  WorldPoint position{};
  /// Who the player who spawns here plays as unless they pick someone else:
  /// a reference into the project's characters table — `character:scout` —
  /// or empty to leave it to the selector.
  ///
  /// The default a level gives a player, not a lock: the character
  /// selector opens on it, and the pick made there is what the run uses.
  /// One naming a character the table no longer has is kept as written, so
  /// a row that goes missing for a while does not cost the start its
  /// character; until it is back the selector opens on the first character.
  std::string character{};
};

}  // namespace eng::editor
