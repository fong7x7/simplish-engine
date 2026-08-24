#pragma once

/// @file dockspace-config-loader.h
/// @brief JSON loader for `GuiDockLayout`. Reads reserved edge sizes;
/// child ids are assigned at runtime via `GuiDockspaceWidget::assignChild`.
///
/// Expected JSON format:
///   {
///     "regions": [
///       { "edge": "TOP",    "size_px": 28 },
///       { "edge": "BOTTOM", "size_px": 200 },
///       { "edge": "LEFT",   "size_px": 0  },
///       { "edge": "RIGHT",  "size_px": 280 },
///       { "edge": "CENTRE", "size_px": 0  }
///     ]
///   }
///
/// Valid `edge` values: "TOP", "BOTTOM", "LEFT", "RIGHT", "CENTRE".
/// `size_px` must be a finite, non-negative number. Unknown edge names,
/// missing fields, negative sizes, and non-finite values all reject the
/// load (returning `std::nullopt`).
///
/// See docs/technical-approaches/engine/gui/dockspace.md §2.2.

#include "gui-dock-layout.h"

#include <optional>
#include <string_view>

namespace eng {

/// Stateless loader for dockspace layout JSON. Main-thread only (called
/// during startup or on hot-reload).
/// @thread_safety Main thread only.
struct DockspaceConfigLoader {
  /// Parse a `GuiDockLayout` from `path`. Returns `std::nullopt` if the
  /// file cannot be opened, fails to parse, or contains invalid field
  /// values. Errors are logged to `Logger::warn` on the "DockspaceConfigLoader"
  /// subsystem. `child` ids in the returned layout are always
  /// `GUI_WIDGET_ID_INVALID`; callers assign them at runtime.
  static std::optional<GuiDockLayout> load(std::string_view path);
};

}  // namespace eng
