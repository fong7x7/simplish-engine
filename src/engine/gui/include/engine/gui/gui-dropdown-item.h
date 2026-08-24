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
};

}  // namespace eng
