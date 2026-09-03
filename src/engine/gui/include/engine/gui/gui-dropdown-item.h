#pragma once

#include <functional>
#include <string>

namespace eng {

/// @brief A single selectable item within a dropdown menu.
/// @thread_safety Immutable value type.
struct GuiDropdownItem {
  /// Display label for this item.
  std::string label{};
  /// Callback invoked when this item is selected.
  std::function<void()> on_select{};
  /// When false, the row is rendered with dimmed text and `selectItem`
  /// is a no-op so disabled rows cannot fire their callback.
  bool enabled = true;
  /// Accelerator hint drawn right-aligned in the row ("Ctrl+S"). Purely
  /// informational: the dropdown never binds keys, so a caller that shows
  /// one is responsible for making the key actually work.
  std::string shortcut{};
  /// When true, the row draws a divider line instead of a label and cannot
  /// be selected or hovered. Separators still occupy a full row, so item
  /// indices and hit testing stay a simple division by the row height.
  bool separator = false;
};

}  // namespace eng
