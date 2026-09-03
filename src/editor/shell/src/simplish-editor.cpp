#include <editor/project/project-ops.h>
#include <editor/shell/simplish-editor.h>
#include <engine/core/logger.h>
#include <engine/gui/gui-label.h>
#include <engine/gui/gui-panel.h>
#include <engine/gui/gui-theme-constants.h>
#include <engine/gui/gui-widget-tree.h>
#include <string>
#include <utility>

namespace eng::editor {

namespace {

  constexpr float TITLE_BAR_HEIGHT = 28.0f;
  constexpr float TITLE_INSET = 10.0f;
  /// How long File > About leaves its line in the toolbar status.
  constexpr float ABOUT_SECONDS = 4.0f;

  /// Zoom the viewport about its own centre, the way a menu command has to:
  /// there is no cursor position to anchor on.
  void zoomAtCentre(EditorViewportWidget& viewport, float notches) {
    zoomCameraAt(viewport.camera, viewport.rect, notches,
                 viewport.rect.x + viewport.rect.w * 0.5f,
                 viewport.rect.y + viewport.rect.h * 0.5f);
  }

  /// Format the toolbar's right-hand status string.
  std::string formatStatus(const EditorViewportWidget& viewport) {
    std::string status = "zoom ";
    status +=
        std::to_string(static_cast<int>(viewport.camera.zoom * 100.0f + 0.5f));
    status += "%";
    if (viewport.hasHover()) {
      const auto tile = viewport.hoveredTile();
      status += "   tile ";
      status += std::to_string(static_cast<int>(tile.x));
      status += ", ";
      status += std::to_string(static_cast<int>(tile.y));
    }
    return status;
  }

}  // namespace

void SimplishEditor::setRecentProjectsPath(std::filesystem::path path) {
  state_.recent_path = std::move(path);
}

bool SimplishEditor::openProjectAt(const std::filesystem::path& root) {
  auto result = openProject(root);
  if (!result.ok()) {
    LOG_ERROR("editor", std::string("Cannot open project: ")
                            .append(projectOpenErrorMessage(result.error)));
    return false;
  }

  state_.project = std::move(result.context);

  const std::string stamp = isoTimestampNow();
  // A failed stamp is not fatal — a read-only project still opens; the write
  // failure is worth a line in the log but not a refusal to load.
  if (!touchProjectOpened(state_.project, stamp)) {
    LOG_WARN("editor", "Could not update last_opened_at in project.json");
  }

  promoteRecentProject(state_.recent, state_.project, stamp);
  if (!state_.recent_path.empty()) {
    (void)saveRecentProjects(state_.recent, state_.recent_path);
  }

  LOG_INFO(
      "editor",
      std::string("Opened project: ").append(state_.project.metadata.name));
  applyProjectToChrome();
  return true;
}

bool SimplishEditor::onInit() {
  // RenderedGameClient::onInit creates the GuiContext (tree, theme stack,
  // text pipeline, renderer). Everything below touches guiWidgetTree(), so
  // this must run first and its failure must abort startup.
  if (!eng::client::RenderedGameClient::onInit()) {
    return false;
  }
  if (!state_.recent_path.empty()) {
    state_.recent = loadRecentProjects(state_.recent_path);
  }
  initChrome();
  applyProjectToChrome();
  layoutChrome();
  return true;
}

void SimplishEditor::initChrome() {
  GuiWidgetTree& tree = guiWidgetTree();
  initRoot(tree);
  initTitleBar(tree);
  initMenuBar(tree);
  initWorkArea(tree);
}

void SimplishEditor::initRoot(GuiWidgetTree& tree) {
  // Nothing creates a root: the tree adopts the first widget attached with
  // no parent as one. Without this panel the title bar became the root, and
  // hit testing — which never descends past a rect that does not contain the
  // cursor — stopped at the 28px title strip, leaving every widget below it
  // unclickable.
  root_panel_ = tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID);
  if (auto* panel = dynamic_cast<GuiPanel*>(tree.findWidget(root_panel_))) {
    panel->fill_color = THEME_BG;
    panel->debug_name = "editor-root";
  }
}

void SimplishEditor::initTitleBar(GuiWidgetTree& tree) {
  title_panel_ = tree.createWidget(GuiWidgetType::PANEL, root_panel_);
  if (auto* panel = dynamic_cast<GuiPanel*>(tree.findWidget(title_panel_))) {
    panel->fill_color = THEME_BG;
    panel->border_color = THEME_BORDER;
    panel->border_width = 1.0f;
    panel->debug_name = "editor-title-bar";
  }
  title_label_ = tree.createWidget(GuiWidgetType::TEXT, title_panel_);
  if (auto* label = dynamic_cast<GuiLabel*>(tree.findWidget(title_label_))) {
    label->color = THEME_TEXT;
    label->align = GuiLabelAlign::LEFT;
    label->text = title_text_;
  }
}

void SimplishEditor::initMenuBar(GuiWidgetTree& tree) {
  auto menu_bar = std::make_unique<EditorMenuBarWidget>();
  menu_bar->on_command = [this](EditorMenuCommand command) {
    executeCommand(command);
  };
  menu_bar->on_open_recent = [this](std::string_view path) {
    (void)openProjectAt(std::filesystem::path(path));
  };
  menu_bar_id_ = tree.insertExternalWidget(std::move(menu_bar), root_panel_);
  if (auto* bar =
          dynamic_cast<EditorMenuBarWidget*>(tree.findWidget(menu_bar_id_))) {
    bar->init(tree);
  }
}

void SimplishEditor::initWorkArea(GuiWidgetTree& tree) {
  auto toolbar = std::make_unique<EditorToolbarWidget>();
  toolbar->on_tool_selected = [this](EditorTool tool) {
    state_.active_tool = tool;
  };
  toolbar_id_ = tree.insertExternalWidget(std::move(toolbar), root_panel_);
  if (auto* bar =
          dynamic_cast<EditorToolbarWidget*>(tree.findWidget(toolbar_id_))) {
    bar->init(tree);
  }
  viewport_id_ = tree.insertExternalWidget(
      std::make_unique<EditorViewportWidget>(), root_panel_);
}

void SimplishEditor::layoutChrome() {
  GuiWidgetTree& tree = guiWidgetTree();
  const Rect window = makeRect(0.0f, 0.0f, static_cast<float>(guiLayoutWidth()),
                               static_cast<float>(guiLayoutHeight()));
  // The root has to cover the window: hit testing starts there and stops
  // dead if the cursor is outside it.
  if (auto* panel = tree.findWidget(root_panel_)) {
    panel->rect = window;
  }
  layoutTitleBar(tree, window);
  layoutWorkArea(tree, window);
  laid_out_width_ = guiLayoutWidth();
  laid_out_height_ = guiLayoutHeight();
}

void SimplishEditor::layoutTitleBar(GuiWidgetTree& tree, const Rect& window) {
  if (auto* panel = tree.findWidget(title_panel_)) {
    panel->rect = makeRect(0.0f, 0.0f, window.w, TITLE_BAR_HEIGHT);
  }
  if (auto* label = tree.findWidget(title_label_)) {
    label->rect = makeRect(TITLE_INSET, TITLE_BAR_HEIGHT * 0.5f - 7.0f,
                           window.w - TITLE_INSET, TITLE_BAR_HEIGHT);
  }
}

void SimplishEditor::layoutWorkArea(GuiWidgetTree& tree, const Rect& window) {
  if (auto* bar =
          dynamic_cast<EditorMenuBarWidget*>(tree.findWidget(menu_bar_id_))) {
    bar->layout(tree,
                makeRect(0.0f, TITLE_BAR_HEIGHT, window.w, MENU_BAR_HEIGHT),
                window);
  }
  const float toolbar_top = TITLE_BAR_HEIGHT + MENU_BAR_HEIGHT;
  if (auto* bar =
          dynamic_cast<EditorToolbarWidget*>(tree.findWidget(toolbar_id_))) {
    bar->layout(tree, makeRect(0.0f, toolbar_top, window.w, TOOLBAR_HEIGHT));
  }
  if (auto* viewport = tree.findWidget(viewport_id_)) {
    const float top = toolbar_top + TOOLBAR_HEIGHT;
    viewport->rect = makeRect(0.0f, top, window.w, window.h - top);
  }
}

void SimplishEditor::applyProjectToChrome() {
  // openProjectAt is callable before run(), and the GUI context is created
  // in onInit() — which run() invokes — so the chrome may not exist yet.
  // A valid toolbar id is the signal that it does; onInit applies the
  // project itself once the chrome is built.
  if (toolbar_id_ == GUI_WIDGET_ID_INVALID) {
    return;
  }
  const bool loaded = state_.project.loaded;
  const std::string& name = state_.project.metadata.name;

  title_text_ = loaded ? ("Simplish Editor — " + name) : "Simplish Editor";
  setWindowTitle(title_text_);

  applyProjectToWidgets();
}

void SimplishEditor::applyProjectToWidgets() {
  const bool loaded = state_.project.loaded;
  GuiWidgetTree& tree = guiWidgetTree();
  if (auto* label = dynamic_cast<GuiLabel*>(tree.findWidget(title_label_))) {
    label->text = title_text_;
  }
  if (auto* bar =
          dynamic_cast<EditorToolbarWidget*>(tree.findWidget(toolbar_id_))) {
    bar->setProjectName(loaded ? state_.project.metadata.name : "No project");
  }
  if (auto* menu =
          dynamic_cast<EditorMenuBarWidget*>(tree.findWidget(menu_bar_id_))) {
    menu->setProjectPresence(loaded ? EditorProjectPresence::OPEN
                                    : EditorProjectPresence::NONE);
    menu->setRecentProjects(state_.recent);
  }
}

void SimplishEditor::refreshToolbar() {
  GuiWidgetTree& tree = guiWidgetTree();
  auto* bar = dynamic_cast<EditorToolbarWidget*>(tree.findWidget(toolbar_id_));
  auto* viewport =
      dynamic_cast<EditorViewportWidget*>(tree.findWidget(viewport_id_));
  if (bar == nullptr || viewport == nullptr) {
    return;
  }
  bar->setStatusText(status_override_left_ > 0.0f ? status_override_
                                                  : formatStatus(*viewport));
  bar->setActiveTool(state_.active_tool);
  bar->tick(tree);
}

bool SimplishEditor::onTick(float dt) {
  // Re-layout only when the window actually changed size; the chrome is
  // placed manually, so an unconditional pass would be wasted work.
  if (guiLayoutWidth() != laid_out_width_ ||
      guiLayoutHeight() != laid_out_height_) {
    layoutChrome();
  }
  if (status_override_left_ > 0.0f) {
    status_override_left_ -= dt;
  }
  GuiWidgetTree& tree = guiWidgetTree();
  if (auto* menu =
          dynamic_cast<EditorMenuBarWidget*>(tree.findWidget(menu_bar_id_))) {
    menu->tick(tree);
  }
  refreshToolbar();
  return !quit_requested_;
}

void SimplishEditor::executeCommand(EditorMenuCommand command) {
  switch (command) {
    case EditorMenuCommand::EXIT:
      quit_requested_ = true;
      return;
    case EditorMenuCommand::CLOSE_PROJECT:
      closeProject();
      return;
    case EditorMenuCommand::ABOUT:
      showAbout();
      return;
    default:
      applyViewCommand(command);
      return;
  }
}

void SimplishEditor::applyViewCommand(EditorMenuCommand command) {
  auto* viewport = dynamic_cast<EditorViewportWidget*>(
      guiWidgetTree().findWidget(viewport_id_));
  if (viewport == nullptr) {
    return;
  }
  if (command == EditorMenuCommand::RESET_VIEW) {
    viewport->camera = IsoCamera{};
  } else if (command == EditorMenuCommand::ZOOM_IN) {
    zoomAtCentre(*viewport, 1.0f);
  } else if (command == EditorMenuCommand::ZOOM_OUT) {
    zoomAtCentre(*viewport, -1.0f);
  } else if (command == EditorMenuCommand::TOGGLE_GRID) {
    viewport->show_grid = !viewport->show_grid;
  }
}

void SimplishEditor::closeProject() {
  if (!state_.project.loaded) {
    return;
  }
  LOG_INFO(
      "editor",
      std::string("Closed project: ").append(state_.project.metadata.name));
  state_.project = ProjectContext{};
  applyProjectToChrome();
}

void SimplishEditor::showAbout() {
  // There is no dialog system yet, so About borrows the toolbar's status
  // line rather than pretending to open a window.
  status_override_ = "Simplish Editor — engine and editor, C++20";
  status_override_left_ = ABOUT_SECONDS;
}

bool SimplishEditor::handleViewKey(uint32_t key) {
  constexpr uint32_t KEY_RESET = '0';
  constexpr uint32_t KEY_ZOOM_IN = '=';
  constexpr uint32_t KEY_ZOOM_OUT = '-';
  if (key == KEY_RESET || key == KEY_ZOOM_IN || key == KEY_ZOOM_OUT) {
    applyViewCommand(key == KEY_RESET     ? EditorMenuCommand::RESET_VIEW
                     : key == KEY_ZOOM_IN ? EditorMenuCommand::ZOOM_IN
                                          : EditorMenuCommand::ZOOM_OUT);
    return true;
  }
  if (key == 'g' || key == 'G') {
    applyViewCommand(EditorMenuCommand::TOGGLE_GRID);
    return true;
  }
  return false;
}

void SimplishEditor::onClientKeyDown(uint32_t key, ClientKeyDownKind kind) {
  if (kind == ClientKeyDownKind::REPEAT) {
    return;
  }
  if (handleViewKey(key)) {
    return;
  }
  // Number keys select tools, matching the toolbar's left-to-right order.
  constexpr uint32_t KEY_1 = '1';
  const auto count = static_cast<uint32_t>(std::size(EDITOR_TOOLS));
  if (key >= KEY_1 && key < KEY_1 + count) {
    state_.active_tool = EDITOR_TOOLS[key - KEY_1];
  }
}

void SimplishEditor::onShutdown() {
  shutdownChrome();
  // Release the GuiContext last: the widgets above live in its tree.
  eng::client::RenderedGameClient::onShutdown();
}

void SimplishEditor::shutdownChrome() {
  GuiWidgetTree& tree = guiWidgetTree();
  // Both widgets own children and siblings the tree does not know to remove
  // with them, so they clean up before their own node is destroyed.
  if (auto* bar =
          dynamic_cast<EditorToolbarWidget*>(tree.findWidget(toolbar_id_))) {
    bar->shutdown(tree);
  }
  if (auto* menu =
          dynamic_cast<EditorMenuBarWidget*>(tree.findWidget(menu_bar_id_))) {
    menu->shutdown(tree);
  }
  destroyChromeWidgets(tree);
}

void SimplishEditor::destroyChromeWidgets(GuiWidgetTree& tree) {
  // The root goes last: destroying it takes every descendant with it.
  tree.destroyWidget(menu_bar_id_);
  tree.destroyWidget(toolbar_id_);
  tree.destroyWidget(viewport_id_);
  tree.destroyWidget(title_panel_);
  tree.destroyWidget(root_panel_);
  menu_bar_id_ = GUI_WIDGET_ID_INVALID;
  toolbar_id_ = GUI_WIDGET_ID_INVALID;
  viewport_id_ = GUI_WIDGET_ID_INVALID;
  title_panel_ = GUI_WIDGET_ID_INVALID;
  title_label_ = GUI_WIDGET_ID_INVALID;
  root_panel_ = GUI_WIDGET_ID_INVALID;
}

}  // namespace eng::editor
