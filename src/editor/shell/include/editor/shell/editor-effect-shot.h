#pragma once

/// @file editor-effect-shot.h
/// @brief One effect to play once, now, wherever the viewport's effects are.
/// @par Threading A value type.

#include <engine/render-fx/fx-burst.h>
#include <engine/render-fx/fx-emit.h>
#include <engine/render-fx/fx-flash.h>
#include <engine/render-fx/fx-volume.h>
#include <vector>

namespace eng::editor {

/// An effect asked for by value rather than by name — its bursts copied in
/// — so it means the same thing whenever it is played: the preset, the
/// combat effect or the emitter it came from may change after it was asked
/// for, and this does not.
///
/// What the agent API's `play_effect` hands the running editor, which plays
/// it into the effects the viewport is drawing: the editor's own while the
/// level is edited, the playtest's while it is played. Presentation: nothing
/// records it, and nothing undoes it.
/// @thread_safety A value type.
struct EditorEffectShot {
  /// The bursts it throws, each from the same place and direction.
  std::vector<FxBurst> bursts{};
  /// The clouds of smoke it leaves standing where it went off.
  std::vector<FxVolume> volumes{};
  /// The flash it lights the scene with; an intensity of zero is none.
  FxFlash flash{};
  /// Where, which way and how big.
  FxEmit emit{};
};

}  // namespace eng::editor
