#pragma once

/// @file editor-emitter-player.h
/// @brief Plays a level's particle emitters: each bursts on its interval.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <editor/shell/editor-emitter.h>
#include <engine/render-fx/fx-world.h>
#include <span>
#include <vector>

namespace eng::editor {

/// Most bursts one emitter throws in one `advance`. A frame the editor
/// spent stalled — a file dialog, a breakpoint — would otherwise come back
/// owing a burst for every interval it missed, all at once.
inline constexpr uint32_t EDITOR_EMITTER_MAX_BURSTS_PER_STEP = 8;

/// When each of a level's emitters bursts next, and the bursts it throws.
///
/// Presentation on the frame's clock, as clips are: nothing is recorded,
/// undone or simulated. Emitters are numbered as the document lists them;
/// one added is seen at once and bursts on that frame, so a dropped emitter
/// shows what it throws the moment it lands.
/// @thread_safety Main-thread-only.
class EditorEmitterPlayer {
public:
  /// Move every one of @p emitters on by @p seconds, throwing into @p world
  /// each burst that comes due — `EDITOR_EMITTER_MAX_BURSTS_PER_STEP` at
  /// most per emitter.
  void advance(std::span<const EditorEmitter> emitters, float seconds,
               FxWorld& world);

  /// Forget when each emitter bursts next, so each bursts the next time it
  /// is advanced — for a level that has been replaced, or played afresh —
  /// and how many bursts each has thrown.
  void reset();

  /// Bursts each emitter has thrown since it was first advanced, or since
  /// the last `reset`, in document order.
  [[nodiscard]] std::span<const uint64_t> bursts() const { return bursts_; }

private:
  /// Seconds until each emitter bursts next, in document order.
  std::vector<float> until_{};
  /// Bursts each emitter has thrown, in document order.
  std::vector<uint64_t> bursts_{};
};

}  // namespace eng::editor
