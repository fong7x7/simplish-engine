#pragma once

/// @file editor-playtest-effects.h
/// @brief The effects in a running playtest, as the editor reports them.
/// @par Threading A value type.

#include <array>
#include <cstdint>
#include <game/combat/combat-cue-kind.h>

namespace eng::editor {

/// What a playtest's effects are doing, for whoever cannot see the
/// viewport: how many particles and flashes are live this frame, and how
/// many cues of each kind the run has played since it started — so an
/// agent that looks after a spark has died can still tell it was thrown.
/// @thread_safety A value type.
struct EditorPlaytestEffects {
  /// Particles alive now.
  uint32_t particles = 0;
  /// Clouds of volumetric smoke standing now.
  uint32_t volumes = 0;
  /// Flashes shining now.
  uint32_t lights = 0;
  /// Cues played since the playtest started, indexed by `CombatCueKind`.
  std::array<uint64_t, game::COMBAT_CUE_KIND_COUNT> cues{};
  /// Cues handed to the speakers since the playtest started: of each
  /// kind a tick left, the few nearest player 1.
  uint64_t sounds = 0;
};

}  // namespace eng::editor
