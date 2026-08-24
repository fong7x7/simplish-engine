#include <editor/shell/editor-toolbar-widget.h>
#include <engine/gui/gui-button.h>
#include <engine/gui/gui-color.h>
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

  GuiButtonStyle activeStyle() {
    return {THEME_ACCENT, THEME_TEXT, THEME_ACCENT_HOVER, THEME_BTN_RADIUS};
  }

  GuiButtonStyle inactiveStyle() {
    return {THEME_BTN, THEME_TEXT, THEME_BTN_HOVER, THEME_BTN_RADIUS};
  }

}  // namespace

EditorToolbarWidget::EditorToolbarWidget() {
  widget_type = GuiWidgetType::PANEL;
  debug_name = "editor-toolbar";
  fill_color = THEME_PANEL;
  border_color = THEME_BORDER;
  border_width = 1.0f;
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
  }

  tool_buttons_.reserve(std::size(EDITOR_TOOLS));
  for (EditorTool tool : EDITOR_TOOLS) {
    GuiWidgetId id = tree.createWidget(GuiWidgetType::BUTTON, bar_panel_);
    if (auto* button = dynamic_cast<GuiButton*>(tree.findWidget(id))) {
      button->label = editorToolLabel(tool);
      button->debug_name = std::string(editorToolLabel(tool));
      button->onClick([this, tool](const GuiMouseEvent&) {
        active_tool_ = tool;
        if (on_tool_selected) {
          on_tool_selected(tool);
        }
      });
    }
    tool_buttons_.push_back(id);
  }

  status_label_ = tree.createWidget(GuiWidgetType::TEXT, bar_panel_);
  if (auto* label = dynamic_cast<GuiLabel*>(tree.findWidget(status_label_))) {
    label->color = THEME_DIM;
    label->align = GuiLabelAlign::LEFT;
    label->text = status_text_;
  }
}

void EditorToolbarWidget::layout(GuiWidgetTree& tree, const Rect& bar_rect) {
  if (bar_panel_ == GUI_WIDGET_ID_INVALID) {
    return;
  }
  rect = bar_rect;

  const float text_y = bar_rect.y + (bar_rect.h * 0.5f) - 7.0f;

  if (auto* label = tree.findWidget(project_label_)) {
    label->rect = makeRect(bar_rect.x + SIDE_PADDING, text_y,
                           PROJECT_LABEL_WIDTH, BUTTON_HEIGHT);
  }

  float cursor_x = bar_rect.x + SIDE_PADDING + PROJECT_LABEL_WIDTH;
  const float button_y = bar_rect.y + (bar_rect.h - BUTTON_HEIGHT) * 0.5f;
  for (GuiWidgetId id : tool_buttons_) {
    if (auto* button = tree.findWidget(id)) {
      button->rect = makeRect(cursor_x, button_y, BUTTON_WIDTH, BUTTON_HEIGHT);
    }
    cursor_x += BUTTON_WIDTH + BUTTON_GAP;
  }

  if (auto* label = tree.findWidget(status_label_)) {
    const float status_x =
        bar_rect.x + bar_rect.w - STATUS_LABEL_WIDTH - SIDE_PADDING;
    label->rect = makeRect(status_x, text_y, STATUS_LABEL_WIDTH, BUTTON_HEIGHT);
  }
}

void EditorToolbarWidget::arrangeChildren(GuiWidgetTree& tree,
                                          const Rect& available) {
  layout(tree, available);
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
}

void EditorToolbarWidget::styleButtons(GuiWidgetTree& tree) {
  for (size_t i = 0; i < tool_buttons_.size(); ++i) {
    auto* button = dynamic_cast<GuiButton*>(tree.findWidget(tool_buttons_[i]));
    if (button == nullptr) {
      continue;
    }
    const bool active = EDITOR_TOOLS[i] == active_tool_;
    button->style = active ? activeStyle() : inactiveStyle();
    button->override_style = true;
  }
}

void EditorToolbarWidget::shutdown(GuiWidgetTree& tree) {
  for (GuiWidgetId id : tool_buttons_) {
    tree.destroyWidget(id);
  }
  tool_buttons_.clear();
  tree.destroyWidget(project_label_);
  tree.destroyWidget(status_label_);
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
