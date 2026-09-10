#pragma once

// Design Summary -- EditorPropertiesWidget
//
// Behaviours:
//   - Column down the right of the viewport listing what the editor has
//     selected: its name, its `kind:id` reference, and one row per
//     editable number
//   - A placed asset lists position X, Y, Z, rotation X, Y, Z, and a
//     Collides checkbox that a click anywhere on its row flips; a light
//     lists the direction, colour, intensity and range its own kind uses;
//     a player start lists its player and its position
//   - Each row is a label, a step-down button, a value box, and a step-up
//     button; a click on a button steps the value, a drag across the value
//     box scrubs it
//   - A rigged model gets one more row, Animation, whose value box names
//     the clip it plays and whose buttons step to the previous or next clip,
//     wrapping round. It is a name, not a number, so it is a row of its own
//     rather than a property field, and reports through on_clip_changed
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
//     there is no longer anything the value belongs to
//   - Panel too short for every row: the rows past the bottom are drawn and
//     hit tested as usual and clipped by the panel, which is what a
//     scrollable panel would do before it had a scrollbar
//   - A field the current selection does not list has no row, and asking
//     for its rect gives an empty one rather than another field's
//
// Invariants:
//   - Hit testing and drawing derive from the same layout functions, so
//     what is drawn is what is pressed
//   - The widget never owns the selection: it is handed a name and a list
//     of values to show and reports edits back. The editor's own document
//     stays the one truth
//   - Every reported value is absolute, not a delta, so a dropped or
//     coalesced event cannot make the value drift from what is on screen
//   - A reported value is already normalised — an angle wrapped, a colour
//     clamped — so what the panel shows is what the document will hold
//
// Integration Points:
//   - SimplishEditor: owns this widget, feeds it the selection, and applies
//     on_property_changed to whatever is selected and on_clip_changed to
//     the selected placement

#include <cstddef>
#include <editor/shell/editor-light.h>
#include <editor/shell/editor-placement.h>
#include <editor/shell/editor-player-start.h>
#include <editor/shell/editor-properties-layout.h>
#include <editor/shell/editor-property-edit.h>
#include <editor/shell/editor-property-field.h>
#include <engine/gui/gui-panel.h>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace eng::editor {

/// The right-hand properties panel for whatever is selected.
/// @thread_safety Main-thread only.
class EditorPropertiesWidget : public GuiPanel {
public:
  EditorPropertiesWidget();

  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// Draw the header, the name and id lines, and every property row.
  void render(const GuiDrawContext& ctx) const override;

  /// Step a value, or begin a scrub. Returns true to capture.
  bool handleMouseDown(const GuiMouseEvent& event) override;

  /// Scrub the value being dragged.
  void handleMouseMove(const GuiMouseEvent& event) override;

  /// Finish a scrub, committing the value it reached.
  void handleMouseUp(const GuiMouseEvent& event) override;

  /// Show a placement's properties. The name is the asset's, for the line
  /// under the header.
  void setSelection(std::string name, const EditorPlacement& placement);

  /// Show a light's properties, which are the ones its kind uses.
  void setSelection(std::string name, const EditorLight& light);

  /// Show a player start's properties: which player, and where.
  void setSelection(std::string name, const EditorPlayerStart& start);

  /// Offer a rigged model's clips as an Animation row below the property
  /// rows, showing the one @p current names — or the first, when it names
  /// none of them, which is the clip such a placement plays. Called after
  /// `setSelection`, which drops any clips a previous selection had; an
  /// empty list shows no row.
  void setClips(std::vector<std::string> clips, const std::string& current);

  /// The clip the Animation row shows, or empty when there is no such row.
  [[nodiscard]] const std::string& clip() const { return clip_; }

  /// Rect of the Animation row, in layout pixels. Empty when there is none.
  [[nodiscard]] Rect clipRowRect() const;

  /// Show nothing, and hide the panel.
  void clearSelection();

  /// Whether anything is being shown.
  [[nodiscard]] bool hasSelection() const { return has_selection_; }

  /// The reference the id line shows — `prop:crate_01` — or empty when
  /// nothing is selected. This is what a level or logic file writes to
  /// name what is selected, which is why the panel shows it qualified
  /// rather than showing the bare id.
  [[nodiscard]] const std::string& reference() const { return reference_; }

  /// The rows the panel is listing, in order.
  [[nodiscard]] const std::vector<EditorPropertyField>& fields() const {
    return fields_;
  }

  /// The value the panel shows for @p field, or zero when it has no such
  /// row.
  [[nodiscard]] float value(EditorPropertyField field) const;

  /// Width the panel wants from the editor's layout: its own width while
  /// something is selected, and nothing at all otherwise.
  [[nodiscard]] float preferredWidth() const;

  /// The panel's regions for its current rect.
  [[nodiscard]] EditorPropertiesLayout layout() const;

  /// Rect of the row for @p field, in layout pixels. Empty when the
  /// selection does not list that field.
  [[nodiscard]] Rect fieldRowRect(EditorPropertyField field) const;

  /// Whether a value is being scrubbed.
  [[nodiscard]] bool dragging() const { return dragging_; }

  /// Raised when the user changes a value: the field, the value it now has,
  /// and whether the gesture that produced it has finished.
  std::function<void(EditorPropertyField, float, EditorPropertyEdit)>
      on_property_changed{};

  /// Raised when a step button on the Animation row picks another clip,
  /// with the name of the clip it picked. Always a finished edit: there is
  /// no gesture to preview.
  std::function<void(const std::string&)> on_clip_changed{};

private:
  /// Draw the title strip.
  void renderHeader(const GuiDrawContext& ctx) const;
  /// Draw the line naming what is selected.
  void renderNameLine(const GuiDrawContext& ctx) const;
  /// Draw the line showing its `kind:id` reference.
  void renderIdLine(const GuiDrawContext& ctx) const;
  /// Draw every property row.
  void renderRows(const GuiDrawContext& ctx) const;
  /// Draw one row's label, buttons, and value.
  void renderRow(const GuiDrawContext& ctx, size_t index) const;
  /// Draw the value box a numeric row @p index shows, in @p row.
  void renderValueBox(const GuiDrawContext& ctx, const Rect& row,
                      size_t index) const;
  /// Draw the checkbox an on-or-off row @p index shows, in @p row.
  void renderToggle(const GuiDrawContext& ctx, const Rect& row,
                    size_t index) const;
  /// Draw the Animation row, when there is one.
  void renderClipRow(const GuiDrawContext& ctx) const;
  /// Draw one step button and its sign.
  void renderStep(const GuiDrawContext& ctx, const Rect& rect,
                  std::string_view sign) const;
  /// Take @p fields as the rows to show, under @p name and @p reference,
  /// with every value zero until the caller fills them in.
  void beginSelection(std::string name, std::string reference,
                      std::span<const EditorPropertyField> fields);
  /// Row @p field sits on, or the row count when it has none.
  [[nodiscard]] size_t rowOf(EditorPropertyField field) const;
  /// Act on a press on one of a row's step buttons. Returns true when one
  /// of them took it.
  bool pressStep(EditorPropertyField field, const Rect& row,
                 const GuiMouseEvent& event);
  /// Start scrubbing @p field from where it is now.
  void beginDrag(EditorPropertyField field, const GuiMouseEvent& event);
  /// Act on a press in row @p index. Returns true when a drag began.
  bool pressRow(size_t index, const GuiMouseEvent& event);
  /// Act on a press in the Animation row: step to the previous or next
  /// clip when it lands on a button.
  void pressClipRow(const GuiMouseEvent& event);
  /// Rows the panel lists: the property rows, and the Animation row when
  /// there are clips.
  [[nodiscard]] size_t rowCount() const;
  /// Move a value by one step and commit it.
  void stepField(EditorPropertyField field, float steps);
  /// Report a value, and remember it as what the panel shows.
  void applyValue(EditorPropertyField field, float value,
                  EditorPropertyEdit edit);

  /// Whether anything is being shown.
  bool has_selection_ = false;
  /// The rows being listed, in order.
  std::vector<EditorPropertyField> fields_{};
  /// What each of those rows shows, indexed alongside `fields_`.
  std::vector<float> values_{};
  /// Backing store for the name line's text.
  std::string name_{};
  /// Backing store for the id line's text: the qualified reference.
  std::string reference_{};
  /// Clip names the Animation row steps through; empty for no such row.
  std::vector<std::string> clips_{};
  /// The clip the Animation row shows.
  std::string clip_{};
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
