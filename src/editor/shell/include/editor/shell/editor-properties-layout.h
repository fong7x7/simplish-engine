#pragma once

/// @file editor-properties-layout.h
/// @brief Where the properties panel's regions, rows, and controls sit.
/// @par Threading Thread-safe (pure functions over value types).

#include <cstddef>
#include <engine/gui/gui-rect.h>

namespace eng::editor {

/// Width of the properties panel down the right-hand side.
///
/// Wide enough for the longest label, a value, and its two step buttons on
/// one line. The panel takes this from the viewport only while something is
/// selected, so an editor with nothing selected is exactly as wide as it
/// was before this panel existed.
inline constexpr float PROPERTIES_PANEL_WIDTH = 236.0f;
/// Height of the panel's title strip.
inline constexpr float PROPERTIES_HEADER_HEIGHT = 22.0f;
/// Height of the line naming the selected asset.
inline constexpr float PROPERTIES_ASSET_HEIGHT = 22.0f;
/// Height of the line showing the selection's id.
///
/// Shorter than the name above it: it is a reference to copy into a level
/// or logic file, not a heading, and it reads as one at a smaller weight.
inline constexpr float PROPERTIES_ID_HEIGHT = 18.0f;
/// Height of one property row.
inline constexpr float PROPERTIES_ROW_HEIGHT = 22.0f;
/// Gap between property rows.
inline constexpr float PROPERTIES_ROW_GAP = 4.0f;
/// Width of a row's label column.
inline constexpr float PROPERTIES_LABEL_WIDTH = 76.0f;
/// Width of one step button.
inline constexpr float PROPERTIES_STEP_WIDTH = 20.0f;
/// Padding between the panel's edges and its contents.
inline constexpr float PROPERTIES_PADDING = 8.0f;

/// The regions a properties panel divides into.
/// @thread_safety Immutable value type.
struct EditorPropertiesLayout {
  /// Title strip across the top.
  Rect header{};
  /// Line naming the selected asset.
  Rect asset{};
  /// Line showing the selection's `kind:id` reference.
  Rect id{};
  /// Area the property rows are laid out down.
  Rect body{};
};

/// Divide a panel into its header, name line, id line, and rows.
///
/// A panel too short for all of it gives what there is to the regions in
/// order and hands the rest zero height, rather than laying rows out past
/// its own bottom edge.
[[nodiscard]] EditorPropertiesLayout layoutEditorProperties(const Rect& panel);

/// Rect of the property row at @p index within @p body.
[[nodiscard]] Rect propertyRowRect(const Rect& body, size_t index);

/// The label column of a row.
[[nodiscard]] Rect propertyLabelRect(const Rect& row);

/// The step-down button at the left of a row's value.
[[nodiscard]] Rect propertyDecrementRect(const Rect& row);

/// The step-up button at the right of a row.
[[nodiscard]] Rect propertyIncrementRect(const Rect& row);

/// The value box between the two step buttons, which is also the surface a
/// drag scrubs.
[[nodiscard]] Rect propertyValueRect(const Rect& row);

/// Index of the row under a point, or -1, among the first @p rows of them.
///
/// Rows are separated by a gap that belongs to neither of them, so a press
/// between two rows hits nothing instead of the nearer one, and a press
/// below the last row of the selection hits nothing rather than a row that
/// is not being shown.
[[nodiscard]] int hitTestPropertyRow(const Rect& body, size_t rows, float x,
                                     float y);

}  // namespace eng::editor
