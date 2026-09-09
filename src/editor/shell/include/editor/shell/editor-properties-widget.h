#pragma once

// Design Summary -- EditorPropertiesWidget
//
// Behaviours:
//   - Column down the right of the viewport listing the selected
//     placement's asset name and its six editable numbers: position X, Y,
//     Z and rotation X, Y, Z
//   - Each row is a label, a step-down button, a value box, and a step-up
//     button; a click on a button steps the value, a drag across the value
//     box scrubs it
//   - A drag reports every intermediate value as a preview and one final
//     value as a commit, so the editor moves the prop live and records one
//     undo entry for the gesture
//   - With nothing selected the panel is hidden entirely, and the viewport
//     has the width back
//
// Edge Cases:
//   - A press that lands in the gap between two rows changes nothing
//   - A drag released outside the panel still commits: the widget holds
//     capture for the whole gesture, and abandoning an edit halfway would
//     leave the document in the state the pointer happened to be over
//   - A selection cleared mid-drag ends the drag without committing, since
//     there is no longer a placement the value belongs to
//   - Panel too short for every row: the rows past the bottom are drawn and
//     hit tested as usual and clipped by the panel, which is what a
//     scrollable panel would do before it had a scrollbar
//
// Invariants:
//   - Hit testing and drawing derive from the same layout functions, so
//     what is drawn is what is pressed
//   - The widget never owns the selection or the placement: it is handed a
//     copy to show and reports edits back. The editor's placement list
//     stays the one truth about the document
//   - Every reported value is absolute, not a delta, so a dropped or
//     coalesced event cannot make the value drift from what is on screen
//
// Integration Points:
//   - SimplishEditor: owns this widget, feeds it the selection, and applies
//     on_property_changed to the selected placement

#include <cstddef>
#include <editor/shell/editor-placement.h>
#include <editor/shell/editor-properties-layout.h>
#include <editor/shell/editor-property-edit.h>
#include <editor/shell/editor-property-field.h>
#include <engine/gui/gui-panel.h>
#include <functional>
#include <memory>
#include <string>

namespace eng::editor {

/// The right-hand properties panel for the selected placement.
/// @thread_safety Main-thread only.
class EditorPropertiesWidget : public GuiPanel {
public:
  EditorPropertiesWidget();

  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// Draw the header, the asset name, and every property row.
  void render(const GuiDrawContext& ctx) const override;

  /// Step a value, or begin a scrub. Returns true to capture.
  bool handleMouseDown(const GuiMouseEvent& event) override;

  /// Scrub the value being dragged.
  void handleMouseMove(const GuiMouseEvent& event) override;

  /// Finish a scrub, committing the value it reached.
  void handleMouseUp(const GuiMouseEvent& event) override;

  /// Show a placement's properties. The name is the asset's, for the line
  /// under the header.
  void setSelection(std::string asset_name, const EditorPlacement& placement);

  /// Show nothing, and hide the panel.
  void clearSelection();

  /// Whether a placement is being shown.
  [[nodiscard]] bool hasSelection() const { return has_selection_; }

  /// The placement being shown, as the panel last had it.
  [[nodiscard]] const EditorPlacement& placement() const { return placement_; }

  /// Width the panel wants from the editor's layout: its own width while
  /// something is selected, and nothing at all otherwise.
  [[nodiscard]] float preferredWidth() const;

  /// The panel's regions for its current rect.
  [[nodiscard]] EditorPropertiesLayout layout() const;

  /// Rect of the row for @p field, in layout pixels.
  [[nodiscard]] Rect fieldRowRect(EditorPropertyField field) const;

  /// Field currently being scrubbed, or nothing when no drag is running.
  [[nodiscard]] bool dragging() const { return dragging_; }

  /// Raised when the user changes a value: the field, the value it now has,
  /// and whether the gesture that produced it has finished.
  std::function<void(EditorPropertyField, float, EditorPropertyEdit)>
      on_property_changed{};

private:
  /// Draw the title strip.
  void renderHeader(const GuiDrawContext& ctx) const;
  /// Draw the line naming the selected asset.
  void renderAssetLine(const GuiDrawContext& ctx) const;
  /// Draw every property row.
  void renderRows(const GuiDrawContext& ctx) const;
  /// Draw one row's label, buttons, and value.
  void renderRow(const GuiDrawContext& ctx, size_t index) const;
  /// Draw one step button and its sign.
  void renderStep(const GuiDrawContext& ctx, const Rect& rect,
                  std::string_view sign) const;
  /// Act on a press on one of a row's step buttons. Returns true when one
  /// of them took it.
  bool pressStep(EditorPropertyField field, const Rect& row,
                 const GuiMouseEvent& event);
  /// Start scrubbing @p field from where it is now.
  void beginDrag(EditorPropertyField field, const GuiMouseEvent& event);
  /// Act on a press in row @p index. Returns true when a drag began.
  bool pressRow(size_t index, const GuiMouseEvent& event);
  /// Move a value by one step and commit it.
  void stepField(EditorPropertyField field, float steps);
  /// Report a value, and remember it as what the panel shows.
  void applyValue(EditorPropertyField field, float value,
                  EditorPropertyEdit edit);

  /// Whether a placement is being shown.
  bool has_selection_ = false;
  /// The placement being shown.
  EditorPlacement placement_{};
  /// Backing store for the asset line's text.
  std::string asset_name_{};
  /// Whether a value is being scrubbed.
  bool dragging_ = false;
  /// Field the scrub is changing.
  EditorPropertyField drag_field_ = EditorPropertyField::POSITION_X;
  /// Value the scrub started from, which every reported value is measured
  /// against so the number tracks total travel rather than accumulating
  /// per-event rounding.
  float drag_start_value_ = 0.0f;
  /// Cursor X the scrub started at.
  float drag_start_x_ = 0.0f;
};

}  // namespace eng::editor
