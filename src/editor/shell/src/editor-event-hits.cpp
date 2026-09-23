#include <editor/shell/editor-animation-event-ops.h>
#include <editor/shell/editor-event-hits.h>
#include <engine/animation/clip-event-crossing.h>
#include <engine/render-sprite/sprite-sheet-frames.h>

namespace eng::editor {

namespace {

  /// The events of the clip @p pass played, or null when @p assets has no
  /// such model, rig or clip.
  const EditorClipEventSet* eventsOf(const EditorClipPass& pass,
                                     const std::vector<EditorAsset>& assets) {
    if (pass.asset >= assets.size() || assets[pass.asset].rig == nullptr ||
        pass.clip >= assets[pass.asset].clip_events.size() ||
        pass.clip >= assets[pass.asset].rig->clips.size()) {
      return nullptr;
    }
    return &assets[pass.asset].clip_events[pass.clip];
  }

  /// When each of @p events falls, in seconds into its clip.
  std::vector<float> timesOf(const std::vector<EditorAnimationEvent>& events) {
    std::vector<float> times;
    times.reserve(events.size());
    for (const EditorAnimationEvent& event : events) {
      times.push_back(event.at);
    }
    return times;
  }

  /// A sheet's frame events a billboard can reach, and when each comes up.
  struct ShownFrames {
    /// The events, on frames the grid has.
    std::vector<const EditorFrameEvent*> events;
    /// Seconds into the sheet's loop each one's frame comes up.
    std::vector<float> times;
  };

  /// The events of @p entry on the first @p frames frames, at @p fps. An
  /// event on a frame the grid does not have is never reached.
  ShownFrames shownFrames(const EditorSheetEventEntry& entry, uint16_t frames,
                          float fps) {
    ShownFrames shown;
    for (const EditorFrameEvent& event : entry.events) {
      if (event.frame < frames) {
        shown.events.push_back(&event);
        shown.times.push_back(static_cast<float>(event.frame) / fps);
      }
    }
    return shown;
  }

}  // namespace

void appendClipEventHits(const EditorClipPass& pass,
                         const std::vector<EditorAsset>& assets,
                         std::vector<EditorEventHit>& hits) {
  const EditorClipEventSet* set = eventsOf(pass, assets);
  if (set == nullptr) {
    return;
  }
  std::vector<size_t> reached;
  animation::crossedClipTimes(timesOf(set->events),
                              assets[pass.asset].rig->clips[pass.clip].duration,
                              pass.window, reached);
  for (const size_t i : reached) {
    hits.push_back(
        {pass.key, pass.at, set->events[i].sound, set->events[i].gain});
  }
}

void appendSheetEventHits(const EditorSprite& sprite,
                          const EditorAnimationEventTable& table,
                          animation::ClipWindow window,
                          std::vector<EditorEventHit>& hits) {
  const EditorSheetEventEntry* entry =
      findEditorSheetEvents(table, sprite.sheet);
  const uint16_t frames = spriteSheetFrameCount(sprite.grid);
  if (entry == nullptr || sprite.grid.fps <= 0.0F || frames == 0) {
    return;
  }
  // A frame's event is the moment the frame comes up, the sheet looping as
  // the billboard does.
  const ShownFrames shown = shownFrames(*entry, frames, sprite.grid.fps);
  std::vector<size_t> reached;
  animation::crossedClipTimes(shown.times,
                              static_cast<float>(frames) / sprite.grid.fps,
                              window, reached);
  const Vec3 at{sprite.position.x, sprite.position.y, sprite.position.z};
  for (const size_t i : reached) {
    hits.push_back(
        {sprite.id, at, shown.events[i]->sound, shown.events[i]->gain});
  }
}

}  // namespace eng::editor
