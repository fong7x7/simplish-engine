#pragma once

/// @file editor-playtest-hazard.h
/// @brief One hazard pool in a running playtest, as the editor reports it.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <editor/shell/iso-projection.h>

namespace eng::editor {

/// A pool on the floor on the tick a playtest last ran: where, how wide,
/// and for how much longer.
/// @thread_safety Main-thread-only.
struct EditorPlaytestHazard {
  /// Its centre, on the floor.
  WorldPoint position{};
  /// Its radius, in tiles.
  float radius = 0.0F;
  /// Ticks it has left.
  uint32_t ticks_left = 0;
};

}  // namespace eng::editor
