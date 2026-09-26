#include <editor/shell/editor-toolbar-widget.h>
#include <engine/gui/gui-button.h>
#include <engine/gui/gui-color.h>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-label.h>
#include <engine/gui/gui-theme-constants.h>
#include <utility>

namespace eng::editor {

namespace {

  constexpr float BUTTON_WIDTH = 72.0f;
  constexpr float BUTTON_HEIGHT = 24.0f;
  constexpr float BUTTON_GAP = 4.0f;
  constexpr float SIDE_PADDING = 10.0f;
  constexpr float PROJECT_LABEL_WIDTH = 220.0f;
  constexpr float STATUS_LABEL_WIDTH = 260.0f;
  /// Margin before the play button, on top of the gap, so the button reads
  /// as a separate thing rather than a sixth tool.
  constexpr float PLAY_BUTTON_GAP = 16.0f;

  /// Give @p widget a fixed @p width and @p height (-1 for its measured
  /// one) that the row never shrinks.
  void fixSize(GuiWidget& widget, float width, float height) {
    widget.tree_layout.width = width;
    widget.tree_layout.height = height;
    widget.tree_layout.flex_shrink = 0.0f;
  }

}  // namespace

EditorToolbarWidget::EditorToolbarWidget() {
  widget_type = GuiWidgetType::PANEL;
  debug_name = "editor-toolbar";
  fill_color = THEME_PANEL;
  border_color = THEME_BORDER;
  border_width = 1.0f;
  // One row, centred up and down: project name, the tools, Play, a spacer
  // that takes whatever is left, and the status line at the right.
  tree_layout.direction = FlexDirection::ROW;
  tree_layout.align_items = Align::CENTER;
  tree_layout.padding = {0.0f, SIDE_PADDING, 0.0f, SIDE_PADDING};
  tree_layout.gap = BUTTON_GAP;
}

std::unique_ptr<GuiWidget> EditorToolbarWidget::clone() const {
  return std::make_unique<EditorToolbarWidget>(*this);
}

void EditorToolbarWidget::init(GuiWidgetTree& tree) {
  if (widget_id == GUI_WIDGET_ID_INVALID) {
    return;
  }
  bar_panel_ = widget_id;
  wireChildren(tree);
  styleButtons(tree);
}

void EditorToolbarWidget::wireChildren(GuiWidgetTree& tree) {
  project_label_ = tree.createWidget(GuiWidgetType::TEXT, bar_panel_);
  if (auto* label = dynamic_cast<GuiLabel*>(tree.findWidget(project_label_))) {
    label->color = THEME_TEXT;
    label->align = GuiLabelAlign::LEFT;
    label->text = project_name_;
    label->role = GuiTextRole::LABEL;
    label->overflow = GuiTextOverflow::ELLIPSIS;
    fixSize(*label, PROJECT_LABEL_WIDTH, -1.0f);
  }

  wireToolButtons(tree);
  wirePlayButton(tree);
  wireSpacer(tree);
  wireStatusLabel(tree);
}

void EditorToolbarWidget::wireStatusLabel(GuiWidgetTree& tree) {
  status_label_ = tree.createWidget(GuiWidgetType::TEXT, bar_panel_);
  if (auto* label = dynamic_cast<GuiLabel*>(tree.findWidget(status_label_))) {
    label->color = THEME_DIM;
    label->align = GuiLabelAlign::LEFT;
    label->text = status_text_;
    label->overflow = GuiTextOverflow::ELLIPSIS;
    // The one thing in the row that gives way on a narrow window.
    label->tree_layout.width = STATUS_LABEL_WIDTH;
  }
}

void EditorToolbarWidget::wireSpacer(GuiWidgetTree& tree) {
  spacer_ = tree.createWidget(GuiWidgetType::PANEL, bar_panel_);
  if (auto* spacer = dynamic_cast<GuiPanel*>(tree.findWidget(spacer_))) {
    spacer->fill_color = GuiColor{0, 0, 0, 0};
    spacer->tree_layout.flex_grow = 1.0f;
  }
}

void EditorToolbarWidget::wireToolButtons(GuiWidgetTree& tree) {
  tool_buttons_.reserve(std::size(EDITOR_TOOLS));
  for (EditorTool tool : EDITOR_TOOLS) {
    GuiWidgetId id = tree.createWidget(GuiWidgetType::BUTTON, bar_panel_);
    if (auto* button = dynamic_cast<GuiButton*>(tree.findWidget(id))) {
      button->label = editorToolLabel(tool);
      button->debug_name = std::string(editorToolLabel(tool));
      fixSize(*button, BUTTON_WIDTH, BUTTON_HEIGHT);
      button->onClick([this, tool](const GuiMouseEvent&) {
        active_tool_ = tool;
        if (on_tool_selected) {
          on_tool_selected(tool);
        }
      });
    }
    tool_buttons_.push_back(id);
  }
}

void EditorToolbarWidget::wirePlayButton(GuiWidgetTree& tree) {
  play_button_ = tree.createWidget(GuiWidgetType::BUTTON, bar_panel_);
  if (auto* button = dynamic_cast<GuiButton*>(tree.findWidget(play_button_))) {
    button->label = "Play";
    button->debug_name = "play";
    fixSize(*button, BUTTON_WIDTH, BUTTON_HEIGHT);
    // Set apart, so it reads as a separate thing rather than another tool.
    button->tree_layout.margin.left = PLAY_BUTTON_GAP;
    button->onClick([this](const GuiMouseEvent&) {
      if (on_play_toggled) {
        on_play_toggled();
      }
    });
  }
}

void EditorToolbarWidget::layout(GuiWidgetTree& tree,
                                 const Rect& bar_rect) const {
  if (bar_panel_ == GUI_WIDGET_ID_INVALID) {
    return;
  }
  tree.measureWidget(bar_panel_, GuiDrawContext{}, bar_rect.w);
  tree.arrangeWidget(bar_panel_, bar_rect);
}

void EditorToolbarWidget::tick(GuiWidgetTree& tree) {
  if (bar_panel_ == GUI_WIDGET_ID_INVALID) {
    return;
  }
  // GuiLabel holds a string_view; re-point it every tick so a reassigned
  // backing string is never left dangling.
  if (auto* label = dynamic_cast<GuiLabel*>(tree.findWidget(project_label_))) {
    label->text = project_name_;
  }
  if (auto* label = dynamic_cast<GuiLabel*>(tree.findWidget(status_label_))) {
    label->text = status_text_;
  }
  styleButtons(tree);
  stylePlayButton(tree);
}

void EditorToolbarWidget::stylePlayButton(GuiWidgetTree& tree) {
  auto* button = dynamic_cast<GuiButton*>(tree.findWidget(play_button_));
  if (button == nullptr) {
    return;
  }
  const bool playing = play_mode_ == EditorPlayMode::PLAYING;
  button->label = playing ? "Stop" : "Play";
  // Lit, in the theme's selected look, while the game runs.
  button->selected = playing;
}

void EditorToolbarWidget::styleButtons(GuiWidgetTree& tree) {
  for (size_t i = 0; i < tool_buttons_.size(); ++i) {
    auto* button = dynamic_cast<GuiButton*>(tree.findWidget(tool_buttons_[i]));
    if (button == nullptr) {
      continue;
    }
    button->selected = EDITOR_TOOLS[i] == active_tool_;
  }
}

void EditorToolbarWidget::shutdown(GuiWidgetTree& tree) {
  for (GuiWidgetId id : tool_buttons_) {
    tree.destroyWidget(id);
  }
  tool_buttons_.clear();
  tree.destroyWidget(play_button_);
  play_button_ = GUI_WIDGET_ID_INVALID;
  tree.destroyWidget(project_label_);
  tree.destroyWidget(status_label_);
  tree.destroyWidget(spacer_);
  spacer_ = GUI_WIDGET_ID_INVALID;
  project_label_ = GUI_WIDGET_ID_INVALID;
  status_label_ = GUI_WIDGET_ID_INVALID;
  bar_panel_ = GUI_WIDGET_ID_INVALID;
}

void EditorToolbarWidget::setProjectName(std::string name) {
  project_name_ = std::move(name);
}

void EditorToolbarWidget::setStatusText(std::string text) {
  status_text_ = std::move(text);
}

void EditorToolbarWidget::setActiveTool(EditorTool tool) {
  active_tool_ = tool;
}

}  // namespace eng::editor
