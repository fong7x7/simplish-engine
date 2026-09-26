#pragma once

// Design Summary -- EditorToolbarWidget
//
// Behaviours:
//   - Horizontal strip below the title bar holding one button per EditorTool
//   - Highlights the active tool and reports selection through onToolSelected
//   - Shows the open project's name on the left and a status string on the
//     right (hovered tile, zoom)
//   - Ends the tool row with a Play button, which reads Stop and takes the
//     accent colour while the level is being played
//
// Edge Cases:
//   - init() without a valid parent: no children are created; tick is a no-op
//   - tick() before init(): no-op (guards on an invalid root id)
//   - Rect narrower than the buttons need: buttons keep their width and
//     overflow is clipped by the parent, rather than collapsing to slivers
//
// Invariants:
//   - GuiWidgetTree is never stored; it is always passed by reference
//   - Label text is backed by strings this widget owns — GuiLabel holds a
//     string_view, so the backing store must outlive the frame
//
// Integration Points:
//   - SimplishEditor: owns this widget, lays it out with the rest of the
//     chrome (GuiWidgetTree::computeLayout), and drives tick()

#include <editor/shell/editor-play-mode.h>
#include <editor/shell/editor-tool.h>
#include <engine/gui/gui-panel.h>
#include <engine/gui/gui-rect.h>
#include <engine/gui/gui-widget-id.h>
#include <engine/gui/gui-widget-tree.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace eng::editor {

/// Height of the toolbar strip in logical pixels.
inline constexpr float TOOLBAR_HEIGHT = 36.0f;

/// Toolbar strip with one button per authoring tool.
/// @thread_safety Main-thread only.
class EditorToolbarWidget : public GuiPanel {
public:
  EditorToolbarWidget();

  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// Create the toolbar panel, project label, tool buttons, and status label.
  void init(GuiWidgetTree& tree);

  /// Lay the toolbar out on its own in @p bar_rect, text measured without
  /// a font. Inside the editor the tree's `computeLayout` does this; the
  /// toolbar is a flex row, styled in its constructor and `init`.
  void layout(GuiWidgetTree& tree, const Rect& bar_rect) const;

  /// Refresh label text and active-tool highlighting.
  void tick(GuiWidgetTree& tree);

  /// Remove every widget this toolbar created from @p tree.
  void shutdown(GuiWidgetTree& tree);

  /// Set the project name shown on the left.
  void setProjectName(std::string name);

  /// Set the status string shown on the right.
  void setStatusText(std::string text);

  /// Currently active tool.
  [[nodiscard]] EditorTool activeTool() const { return active_tool_; }

  /// Set the active tool without invoking the selection callback.
  void setActiveTool(EditorTool tool);

  /// Show whether the level is being played: the play button reads Play
  /// or Stop, and is lit while playing.
  void setPlayMode(EditorPlayMode mode) { play_mode_ = mode; }

  /// Called when the user clicks a tool button.
  std::function<void(EditorTool)> on_tool_selected{};

  /// Called when the user clicks the play button, whichever it reads.
  std::function<void()> on_play_toggled{};

private:
  /// Create the child widgets under the toolbar panel.
  void wireChildren(GuiWidgetTree& tree);
  /// Apply active/inactive styling to each tool button.
  void styleButtons(GuiWidgetTree& tree);
  /// Create the status label at the right end of the row.
  void wireStatusLabel(GuiWidgetTree& tree);
  /// Create the empty, growing panel that pushes the status label right.
  void wireSpacer(GuiWidgetTree& tree);
  /// Make @p tool the active one, as its button's click does.
  void pickTool(EditorTool tool);
  /// Create one button per tool.
  void wireToolButtons(GuiWidgetTree& tree);
  /// Create the play button at the end of the tool row.
  void wirePlayButton(GuiWidgetTree& tree);
  /// Label and light the play button for the current mode.
  void stylePlayButton(GuiWidgetTree& tree);

  /// Root panel for the strip.
  GuiWidgetId bar_panel_ = GUI_WIDGET_ID_INVALID;
  /// Project-name label, left-aligned.
  GuiWidgetId project_label_ = GUI_WIDGET_ID_INVALID;
  /// Status label, right-aligned.
  GuiWidgetId status_label_ = GUI_WIDGET_ID_INVALID;
  /// Empty panel before the status label that grows to push it right.
  GuiWidgetId spacer_ = GUI_WIDGET_ID_INVALID;
  /// One button per entry in EDITOR_TOOLS, in the same order.
  std::vector<GuiWidgetId> tool_buttons_{};
  /// The Play / Stop button, after the tools.
  GuiWidgetId play_button_ = GUI_WIDGET_ID_INVALID;
  /// Whether the level is being played, which is what the button reads.
  EditorPlayMode play_mode_ = EditorPlayMode::EDITING;
  /// Active tool.
  EditorTool active_tool_ = EditorTool::SELECT;
  /// Backing store for the project label's string_view.
  std::string project_name_{"No project"};
  /// Backing store for the status label's string_view.
  std::string status_text_{};
};

}  // namespace eng::editor
