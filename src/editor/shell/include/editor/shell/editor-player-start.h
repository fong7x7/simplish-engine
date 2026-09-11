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
  /// What the player who spawns here looks like: the reference of the
  /// asset drawn for them — `mesh:characters_hero`, `shape:cylinder` — or
  /// empty for the stand-in.
  ///
  /// A reference rather than an index into the asset list, which a rescan
  /// renumbers, and the qualified one rather than the bare id, because a
  /// model and a built-in shape may share an id. One naming an asset the
  /// project no longer has is kept as written, so a file that goes missing
  /// for a while does not cost the start its character, and is drawn as the
  /// stand-in until it is back.
  std::string character{};
};

}  // namespace eng::editor
