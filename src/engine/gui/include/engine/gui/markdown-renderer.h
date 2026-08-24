#pragma once

/// @file markdown-renderer.h
/// @brief Converts Markdown block AST into a GuiWidgetTree subtree.
/// @par Threading Main thread only.
///
/// Creates panels and labels under a parent widget, styled according to
/// MarkdownRenderConfig.  Supports streaming updates by re-rendering only
/// blocks that changed since the previous call.

#include "gui-widget-id.h"
#include "markdown-block.h"
#include "markdown-render-config.h"

#include <cstddef>
#include <span>

namespace eng {
class GuiWidgetTree;
}  // namespace eng

namespace eng::gui {

/// Groups parameters for a single Markdown render call.
/// @thread_safety Main thread only.
struct MarkdownRenderParams {
  /// Widget ID under which to create the rendered subtree.
  GuiWidgetId parent_id = GUI_WIDGET_ID_INVALID;
  /// Style and layout configuration (nullptr uses static defaults).
  const MarkdownRenderConfig* config = nullptr;
  /// Number of blocks already rendered in a previous call.
  /// Set to 0 for a full re-render; set to the prior block count for
  /// streaming updates (only tail blocks are re-rendered).
  std::size_t previous_complete_count = 0;
};

/// Converts a Markdown block AST into GuiWidgetTree child nodes.
/// @thread_safety Main thread only.
struct MarkdownRenderer {
  /// Render markdown blocks as child widgets under params.parent_id.
  /// When params.previous_complete_count > 0, destroys the last rendered
  /// block and re-renders from that index onward (streaming optimisation).
  static void render(GuiWidgetTree& tree, const MarkdownRenderParams& params,
                     std::span<const MarkdownBlock> blocks);

  /// Remove all child widgets under the given parent.
  static void clear(GuiWidgetTree& tree, GuiWidgetId parent_id);
};

}  // namespace eng::gui
