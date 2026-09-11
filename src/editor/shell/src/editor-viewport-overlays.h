#pragma once

/// @file editor-viewport-overlays.h
/// @brief Drawing the navigation and AI overlays over the viewport.
/// @par Threading Main-thread-only.

#include <editor/shell/editor-actor-overlay.h>
#include <editor/shell/editor-nav-overlay.h>
#include <editor/shell/iso-projection.h>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-renderer.h>
#include <span>

namespace eng::editor {

/// Draw every run of @p overlay as a translucent band on the floor: red
/// where props are, amber where it is too narrow to stand, violet where no
/// player start reaches.
void renderNavOverlay(GuiRendererContext& renderer, const IsoView& view,
                      const EditorNavOverlay& overlay);

/// Draw each of @p actors' view cone, path and line to its target.
void renderActorOverlays(GuiRendererContext& renderer, const IsoView& view,
                         std::span<const EditorActorOverlay> actors);

/// Write each of @p actors' label over its head.
void renderActorLabels(const GuiDrawContext& ctx, const IsoView& view,
                       std::span<const EditorActorOverlay> actors);

}  // namespace eng::editor
