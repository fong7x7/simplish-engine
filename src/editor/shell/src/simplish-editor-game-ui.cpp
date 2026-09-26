#include <algorithm>
#include <editor/project/project-paths.h>
#include <editor/project/project-text-file.h>
#include <editor/shell/editor-widgets-json.h>
#include <editor/shell/simplish-editor.h>
#include <engine/client/desktop-platform-keycode.h>
#include <engine/core/logger.h>
#include <engine/gui/gui-nav-buttons.h>
#include <engine/gui/gui-panel.h>
#include <engine/gui/gui-software-rasterizer.h>
#include <filesystem>
#include <game/ui/ui-actions.h>
#include <game/ui/ui-screen-render.h>

namespace eng::editor {

namespace {

  using Keycode = eng::client::DesktopPlatformKeycode;

  /// The menu command an arrow, Return or Space is, while a game menu is
  /// open; nothing for any other key.
  std::optional<GuiNavCommand> menuCommandFor(uint32_t key) {
    switch (key) {
      case Keycode::ARROW_UP:
        return GuiNavCommand::UP;
      case Keycode::ARROW_DOWN:
        return GuiNavCommand::DOWN;
      case Keycode::ARROW_LEFT:
        return GuiNavCommand::LEFT;
      case Keycode::ARROW_RIGHT:
        return GuiNavCommand::RIGHT;
      case Keycode::KEY_RETURN:
      case Keycode::SPACE:
        return GuiNavCommand::CONFIRM;
      case Keycode::TAB:
        return GuiNavCommand::NEXT;
      default:
        return std::nullopt;
    }
  }

}  // namespace

void SimplishEditor::initGameUi(GuiWidgetTree& tree) {
  auto& layer = *dynamic_cast<GuiPanel*>(
      tree.findWidget(tree.createWidget(GuiWidgetType::PANEL, stage_panel_)));
  layer.fill_color = {0, 0, 0, 0};
  layer.pointer_through = true;
  layer.debug_name = "game ui";
  game_ui_id_ = layer.widget_id;
  coverStage(tree, game_ui_id_);
}

void SimplishEditor::syncGameUi() {
  const game::WorldUi& ui = playtest_->ui();
  bool relayout = false;
  if (ui.open != game_ui_shown_) {
    rebuildGameUi(ui);
    relayout = true;
  }
  for (const auto& view : game_ui_views_) {
    relayout = view->apply(guiWidgetTree(), ui.values) || relayout;
  }
  if (relayout) {
    layoutChrome();
  }
  publishGameUi();
}

void SimplishEditor::rebuildGameUi(const game::WorldUi& ui) {
  clearGameUi();
  for (const std::string& id : ui.open) {
    const game::UiScreen* screen = findEditorUiScreen(state_.ui, id);
    if (screen == nullptr) {
      continue;
    }
    auto view = std::make_unique<game::UiScreenView>(
        *screen, [this](std::string_view action) { chooseUiAction(action); },
        state_.ui.theme);
    (void)view->build(guiWidgetTree(), game_ui_id_);
    game_ui_views_.push_back(std::move(view));
  }
  game_ui_shown_ = ui.open;
  focusGameMenu();
}

void SimplishEditor::clearGameUi() {
  GuiWidgetTree& tree = guiWidgetTree();
  for (const auto& view : game_ui_views_) {
    view->destroy(tree);
  }
  game_ui_views_.clear();
  game_ui_shown_.clear();
  tree.setFocusScope(GUI_WIDGET_ID_INVALID);
}

void SimplishEditor::focusGameMenu() {
  const game::UiScreenView* menu = gameMenu();
  GuiWidgetTree& tree = guiWidgetTree();
  if (menu == nullptr) {
    return;
  }
  // The pad and the arrows move within the menu on top, and start on its
  // first button, shown, so a player sees where they are.
  tree.setFocusScope(menu->overlay());
  if (menu->firstButton() != GUI_WIDGET_ID_INVALID) {
    tree.setFocus(menu->firstButton());
    tree.focus_visibility = GuiFocusVisibility::SHOWN;
  }
}

const game::UiScreenView* SimplishEditor::gameMenu() const {
  const auto top = std::ranges::find_if(
      game_ui_views_.rbegin(), game_ui_views_.rend(), [](const auto& view) {
        return view->screen().layer == game::UiScreenLayer::MENU;
      });
  return top == game_ui_views_.rend() ? nullptr : top->get();
}

bool SimplishEditor::handleGameUiKey(uint32_t key) {
  const std::optional<GuiNavCommand> command = menuCommandFor(key);
  if (!isPlaying() || gameMenu() == nullptr || !command) {
    return false;
  }
  guiWidgetTree().focus_visibility = GuiFocusVisibility::SHOWN;
  (void)guiWidgetTree().routeNav(*command);
  return true;
}

bool SimplishEditor::handleGameUiButton(input::GamepadButton button) {
  const std::optional<GuiNavCommand> command =
      guiNavCommandFor(button, gamepads().activeFamily());
  if (!isPlaying() || gameMenu() == nullptr || !command) {
    return false;
  }
  guiWidgetTree().focus_visibility = GuiFocusVisibility::SHOWN;
  (void)guiWidgetTree().routeNav(*command);
  return true;
}

bool SimplishEditor::chooseUiAction(std::string_view action) {
  const std::vector<std::string> actions = game::uiActions(state_.ui.screens);
  const auto found = std::ranges::find(actions, action);
  if (!isPlaying() || found == actions.end()) {
    return false;
  }
  playtest_->queueUiAction(static_cast<uint32_t>(found - actions.begin()) + 1U);
  return true;
}

void SimplishEditor::takeAgentUiPress() {
  std::string& press = state_.playtest.ui.press;
  if (!press.empty() && !chooseUiAction(press)) {
    LOG_WARN("editor", "press_ui: no screen names the action '" + press + "'");
  }
  press.clear();
}

void SimplishEditor::publishGameUi() {
  EditorPlaytestUi& ui = state_.playtest.ui;
  ui.open = playtest_->ui().open;
  ui.values = playtest_->ui().values;
  ui.buttons.clear();
  ui.nodes.clear();
  for (const auto& view : game_ui_views_) {
    std::ranges::move(view->nodes(guiWidgetTree()),
                      std::back_inserter(ui.nodes));
    for (game::UiButtonInfo& button : view->buttons(guiWidgetTree())) {
      ui.buttons.push_back({view->screen().id, std::move(button.id),
                            std::move(button.action), std::move(button.text),
                            button.rect});
    }
  }
}

bool SimplishEditor::writeUiScreen(std::string_view id, std::string_view text) {
  if (!state_.project.loaded || !editorUiScreenIdValid(id) ||
      !writeProjectTextFile(editorUiScreenPath(state_.project.root, id),
                            text)) {
    return false;
  }
  reloadUi();
  return true;
}

bool SimplishEditor::writeUiTheme(std::string_view text) {
  if (!state_.project.loaded ||
      !writeProjectTextFile(editorUiThemePath(state_.project.root), text)) {
    return false;
  }
  reloadUi();
  // Shown screens hold the theme they were built in: build them again.
  game_ui_shown_.clear();
  return true;
}

void SimplishEditor::describeWidgets(const EditorWidgetQuery& query) {
  state_.widgets = editorWidgetsJson(guiWidgetTree(), query);
}

void SimplishEditor::renderUiScreen(std::string_view id,
                                    game::UiRenderSize size,
                                    const game::UiValues& values) {
  EditorUiRender& out = state_.ui_render;
  out = {.id = std::string(id)};
  const game::UiScreen* screen = findEditorUiScreen(state_.ui, id);
  if (!state_.project.loaded || screen == nullptr) {
    out.error = "no screen called '" + std::string(id) + "' in content/ui/";
    return;
  }
  keepUiRender(game::renderUiScreen(*screen, values, size, state_.ui.theme));
}

void SimplishEditor::keepUiRender(const game::UiScreenRender& render) {
  EditorUiRender& out = state_.ui_render;
  const std::filesystem::path path =
      projectBuildPath(state_.project.root) / "ui" / (out.id + ".png");
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (!GuiSoftwareRasterizer::writePng(render.image, path.string())) {
    out.error = "could not write " + path.string();
    return;
  }
  out.path = path.string();
  out.width = render.image.width;
  out.height = render.image.height;
  out.text = render.text;
  out.buttons = render.buttons;
  out.nodes = render.nodes;
}

}  // namespace eng::editor
