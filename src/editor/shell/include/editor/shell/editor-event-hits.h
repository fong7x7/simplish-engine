#pragma once

/// @file editor-event-hits.h
/// @brief Which animation events a frame's playback reached.
/// @par Threading Thread-safe (pure functions over value types).

#include <editor/shell/editor-animation-event-table.h>
#include <editor/shell/editor-asset.h>
#include <editor/shell/editor-clip-pass.h>
#include <editor/shell/editor-event-hit.h>
#include <editor/shell/editor-sprite.h>
#include <engine/animation/clip-window.h>
#include <vector>

namespace eng::editor {

/// Append to @p hits every event of the clip @p pass played that the pass
/// reached, from @p assets' events for it (`EditorAsset::clip_events`), in
/// the order reached.
void appendClipEventHits(const EditorClipPass& pass,
                         const std::vector<EditorAsset>& assets,
                         std::vector<EditorEventHit>& hits);

/// Append to @p hits every frame event of @p sprite's sheet in @p table
/// that the billboard reached over @p window of the sprite clock — where a
/// frame's event fires as the frame comes up. A billboard held on its
/// first frame (no speed) reaches none.
void appendSheetEventHits(const EditorSprite& sprite,
                          const EditorAnimationEventTable& table,
                          animation::ClipWindow window,
                          std::vector<EditorEventHit>& hits);

}  // namespace eng::editor
