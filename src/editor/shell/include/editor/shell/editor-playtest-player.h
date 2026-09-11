#pragma once

/// @file editor-playtest-player.h
/// @brief One player in a running playtest, as the editor reports it.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <editor/shell/iso-projection.h>
#include <string>

namespace eng::editor {

/// Where one player is on the tick a playtest last ran — what the agent
/// API reports, copied out of the simulation rather than read from it.
/// @thread_safety Main-thread-only.
struct EditorPlaytestPlayer {
  /// Which player, 1 to 4.
  uint8_t player = 1;
  /// Where the player's feet are, in tiles.
  WorldPoint position{};
  /// The asset reference the player is drawn as, or empty for the
  /// stand-in.
  std::string character{};
};

}  // namespace eng::editor
