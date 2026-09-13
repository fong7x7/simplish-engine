#include <algorithm>
#include <editor/shell/editor-emitter-ops.h>
#include <editor/shell/editor-emitter-player.h>
#include <engine/render-fx/fx-effect.h>

namespace eng::editor {

namespace {

  /// Throw one of @p emitter's bursts into @p world, and light its flash.
  void burst(const EditorEmitter& emitter, FxWorld& world) {
    const FxEffect effect{std::span<const FxBurst>(&emitter.burst, 1),
                          emitter.flash};
    playFxEffect(world, effect, editorEmitterEmit(emitter));
  }

  /// Move @p emitter on by @p seconds, with @p until the time to its next
  /// burst, throwing each that comes due. How many it threw.
  uint32_t advanceOne(const EditorEmitter& emitter, float& until, float seconds,
                      FxWorld& world) {
    const float interval =
        std::max(emitter.interval, EDITOR_EMITTER_MIN_INTERVAL);
    until -= seconds;
    uint32_t thrown = 0;
    while (until <= 0.0f && thrown < EDITOR_EMITTER_MAX_BURSTS_PER_STEP) {
      burst(emitter, world);
      until += interval;
      ++thrown;
    }
    // What a stall owed past the cap is let go, not carried: the next
    // burst is a whole interval away.
    if (until <= 0.0f) {
      until = interval;
    }
    return thrown;
  }

}  // namespace

void EditorEmitterPlayer::advance(std::span<const EditorEmitter> emitters,
                                  float seconds, FxWorld& world) {
  // One added since the last frame starts at zero, which is due now.
  until_.resize(emitters.size(), 0.0f);
  bursts_.resize(emitters.size(), 0);
  for (size_t i = 0; i < emitters.size(); ++i) {
    bursts_[i] += advanceOne(emitters[i], until_[i], seconds, world);
  }
}

void EditorEmitterPlayer::reset() {
  until_.clear();
  bursts_.clear();
}

}  // namespace eng::editor
