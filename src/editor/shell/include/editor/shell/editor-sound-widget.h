#pragma once

/// @file editor-sound-widget.h
/// @brief The screen a user sets the volume and the project's sounds on.
/// @par Threading Main-thread-only.
///
/// Behaviours:
///   - Covers the viewport while open, dimming what is behind it, with a
///     panel of rows: the user's volumes as bars and the mute, then each of
///     the game's sounds and the project file it plays
///   - One row is highlighted, never a heading; arrows or the d-pad move
///     it, and the hint under the rows says what the keys do on it
///   - More rows than fit scroll: the highlighted row is always in view,
///     the wheel scrolls, and a thin bar on the right says where the view
///     is in the list
///   - A click on a row highlights it; a click on a volume's bar sets it
///     to where it was clicked; a click off the panel closes the screen
///
/// Invariants:
///   - The widget never changes a setting itself: it shows rows it is given
///     and reports what was picked; the editor changes, saves, and hands it
///     fresh rows

#include <cstddef>
#include <editor/shell/editor-sound-row.h>
#include <engine/gui/gui-panel.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace eng::editor {

/// The Sound screen, drawn over the viewport.
/// @thread_safety Main-thread only.
class EditorSoundWidget : public GuiPanel {
public:
  EditorSoundWidget();

  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// Show @p rows with the first that is not a heading highlighted.
  void open(std::vector<EditorSoundRow> rows);

  /// Replace the rows shown, keeping the highlight — after a change.
  void refresh(std::vector<EditorSoundRow> rows);

  /// Hide the screen.
  void close();

  /// Whether the screen is showing.
  [[nodiscard]] bool isOpen() const { return !rows_.empty(); }

  /// The row highlighted.
  [[nodiscard]] size_t highlighted() const { return highlighted_; }

  /// The rows showing.
  [[nodiscard]] const std::vector<EditorSoundRow>& rows() const {
    return rows_;
  }

  /// Move the highlight @p steps rows that are not headings, wrapping.
  void moveHighlight(int steps);

  /// The panel the rows sit in, for the current rect.
  [[nodiscard]] Rect panelRect() const;

  /// Row @p index's rect, where it is drawn with the list scrolled as it
  /// is. Meaningless for a row scrolled out of view.
  [[nodiscard]] Rect rowRect(size_t index) const;
  /// Whether row @p index is scrolled into view.
  [[nodiscard]] bool rowShown(size_t index) const;
  /// The first row in view.
  [[nodiscard]] size_t firstShown() const { return first_shown_; }

  /// Row @p index's volume bar, when it is a volume.
  [[nodiscard]] Rect barRect(size_t index) const;

  /// Draw the backdrop, the panel, its hint and every row.
  void render(const GuiDrawContext& ctx) const override;

  /// Highlight the row clicked, set a volume from its bar, or close on a
  /// click off the panel.
  bool handleMouseDown(const GuiMouseEvent& event) override;
  /// Scroll the list a few rows, when it is longer than the panel.
  bool handleScroll(const GuiScrollEvent& event) override;

  /// Raised when a click off the panel dismisses the screen.
  std::function<void()> on_dismissed;

  /// Raised with a volume row and the level, 0 to 1, its bar was clicked
  /// at.
  std::function<void(size_t, float)> on_level_picked;

private:
  /// The line under the rows: what the keys do on the highlighted row.
  [[nodiscard]] std::string hint() const;
  /// Draw row @p index.
  void renderRow(const GuiDrawContext& ctx, size_t index) const;
  /// Draw row @p index, which is not a heading: its highlight, name and
  /// value or bar.
  void renderEntry(const GuiDrawContext& ctx, size_t index) const;
  /// A click on row @p index, @p x across: highlight it, and set a volume
  /// from its bar.
  void clickRow(size_t index, float x);
  /// Draw volume row @p index's bar.
  void renderBar(const GuiDrawContext& ctx, size_t index) const;
  /// How many rows fit in the panel at once.
  [[nodiscard]] size_t rowsFitting() const;
  /// Scroll so the highlighted row is in view.
  void reveal();
  /// Scroll so row @p first is the first in view, held to the list.
  void scrollTo(long first);
  /// Draw the rows in view, and the scroll bar beside them.
  void renderRows(const GuiDrawContext& ctx) const;
  /// Draw where the view is in a list longer than it.
  void renderScrollbar(const GuiDrawContext& ctx) const;
  /// The first row at or after @p from, stepping @p step, that is not a
  /// heading; @p from itself when every row is one.
  [[nodiscard]] size_t selectable(size_t from, int step) const;

  /// The rows showing.
  std::vector<EditorSoundRow> rows_;
  /// Which is highlighted.
  size_t highlighted_ = 0;
  /// The first row in view.
  size_t first_shown_ = 0;
};

}  // namespace eng::editor
