#pragma once

/// @file editor-asset-browser-layout-params.h
/// @brief What laying out an asset browser needs beyond its rect.
/// @par Threading Thread-safe (immutable value type).

#include <engine/gui/gui-rect.h>

namespace eng::editor {

/// A panel's rect, and which of its parts are folded away.
/// @thread_safety Immutable value type.
struct EditorAssetBrowserLayoutParams {
  /// The whole panel.
  Rect panel{};
  /// Whether the folder pane is folded away, leaving the cards the width.
  bool nav_collapsed = false;
  /// Whether the panel is folded down to its header.
  bool panel_collapsed = false;
};

}  // namespace eng::editor
