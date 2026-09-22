#pragma once

/// @file editor-effects-state.h
/// @brief The viewport's effects, as far as the rest of the editor can see
/// them.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <vector>

namespace eng::editor {

/// What the effects the viewport draws are doing, refreshed by the editor
/// every frame — a mirror, as `EditorPlaytestState` is, so the agent API can
/// read it without holding the effects themselves. The level's emitters'
/// while it is being edited, the playtest's while it is being played.
/// @thread_safety Main-thread-only.
struct EditorEffectsState {
  /// Particles alive now.
  uint32_t particles = 0;
  /// Clouds of volumetric smoke standing now.
  uint32_t volumes = 0;
  /// Flashes shining now.
  uint32_t lights = 0;
  /// Bursts each emitter has thrown, in document order, since it was
  /// placed, since the level was opened, or since a playtest last started
  /// or stopped — whichever was latest.
  std::vector<uint64_t> emitter_bursts{};
  /// Effects played on request, by `play_effect`, since the editor opened.
  uint64_t shots_played = 0;
};

}  // namespace eng::editor
