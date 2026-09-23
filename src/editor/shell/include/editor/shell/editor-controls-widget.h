#pragma once

/// @file editor-controls-widget.h
/// @brief The screen a user remaps their keys and pad on.
/// @par Threading Main-thread-only.
///
/// Behaviours:
///   - Covers the viewport while open, dimming what is behind it, with a
///     panel listing every action: its name, its keys, its pad controls
///   - One row is highlighted; arrows or the d-pad move it, and Enter or
///     the pad's confirm button starts listening for that action — which
///     the editor routes here, as it does the character selector's keys
///   - While listening, the next key or pad control pressed is bound to
///     the action, replacing its controls on that device (Esc stops)
///   - A click on a row highlights it and starts listening; a click off
///     the panel closes the screen
///
/// Invariants:
///   - The widget never changes a binding itself: it shows rows it is
///     given and reports which row was picked; the editor rebinds, saves,
///     and hands it fresh rows

#include <cstddef>
#include <editor/shell/editor-controls-mode.h>
#include <editor/shell/editor-controls-row.h>
#include <engine/gui/gui-panel.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace eng::editor {

/// The Controls screen, drawn over the viewport.
/// @thread_safety Main-thread only.
class EditorControlsWidget : public GuiPanel {
public:
  EditorControlsWidget();

  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// Show @p rows, browsing, with the first highlighted.
  void open(std::vector<EditorControlsRow> rows);

  /// Replace the rows shown, keeping the highlight and going back to
  /// browsing — after a rebind.
  void refresh(std::vector<EditorControlsRow> rows);

  /// Hide the screen.
  void close();

  /// Whether the screen is showing.
  [[nodiscard]] bool isOpen() const { return !rows_.empty(); }

  /// Browsing or listening.
  [[nodiscard]] EditorControlsMode mode() const { return mode_; }

  /// The row highlighted.
  [[nodiscard]] size_t highlighted() const { return highlighted_; }

  /// The rows showing.
  [[nodiscard]] const std::vector<EditorControlsRow>& rows() const {
    return rows_;
  }

  /// Move the highlight @p steps rows, wrapping round.
  void moveHighlight(int steps);

  /// Start listening for the highlighted row's new control.
  void listen();

  /// Stop listening without binding anything.
  void stopListening() { mode_ = EditorControlsMode::BROWSING; }

  /// The panel the rows sit in, for the current rect.
  [[nodiscard]] Rect panelRect() const;

  /// Row @p index's rect.
  [[nodiscard]] Rect rowRect(size_t index) const;

  /// Draw the backdrop, the panel, its hint and every row.
  void render(const GuiDrawContext& ctx) const override;

  /// Listen on the row clicked, or close on a click off the panel.
  bool handleMouseDown(const GuiMouseEvent& event) override;

  /// Raised when a click off the panel dismisses the screen.
  std::function<void()> on_dismissed;

private:
  /// The line under the rows: what the keys do, or what is being listened
  /// for.
  [[nodiscard]] std::string hint() const;

  /// Draw row @p index.
  void renderRow(const GuiDrawContext& ctx, size_t index) const;

  /// The rows showing.
  std::vector<EditorControlsRow> rows_;
  /// Which is highlighted.
  size_t highlighted_ = 0;
  /// Browsing or listening.
  EditorControlsMode mode_ = EditorControlsMode::BROWSING;
};

}  // namespace eng::editor
