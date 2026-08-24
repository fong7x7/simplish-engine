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
  const GuiWidgetId root = tree.root_id;

  title_panel_ = tree.createWidget(GuiWidgetType::PANEL, root);
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

  auto toolbar = std::make_unique<EditorToolbarWidget>();
  toolbar->on_tool_selected = [this](EditorTool tool) {
    state_.active_tool = tool;
  };
  toolbar_id_ = tree.insertExternalWidget(std::move(toolbar), root);
  if (auto* bar =
          dynamic_cast<EditorToolbarWidget*>(tree.findWidget(toolbar_id_))) {
    bar->init(tree);
  }

  viewport_id_ =
      tree.insertExternalWidget(std::make_unique<EditorViewportWidget>(), root);
}

void SimplishEditor::layoutChrome() {
  GuiWidgetTree& tree = guiWidgetTree();
  const auto width = static_cast<float>(guiLayoutWidth());
  const auto height = static_cast<float>(guiLayoutHeight());

  if (auto* panel = tree.findWidget(title_panel_)) {
    panel->rect = makeRect(0.0f, 0.0f, width, TITLE_BAR_HEIGHT);
  }
  if (auto* label = tree.findWidget(title_label_)) {
    label->rect = makeRect(TITLE_INSET, TITLE_BAR_HEIGHT * 0.5f - 7.0f,
                           width - TITLE_INSET, TITLE_BAR_HEIGHT);
  }
  if (auto* bar =
          dynamic_cast<EditorToolbarWidget*>(tree.findWidget(toolbar_id_))) {
    bar->layout(tree, makeRect(0.0f, TITLE_BAR_HEIGHT, width, TOOLBAR_HEIGHT));
  }
  if (auto* viewport = tree.findWidget(viewport_id_)) {
    const float top = TITLE_BAR_HEIGHT + TOOLBAR_HEIGHT;
    viewport->rect = makeRect(0.0f, top, width, height - top);
  }

  laid_out_width_ = guiLayoutWidth();
  laid_out_height_ = guiLayoutHeight();
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

  GuiWidgetTree& tree = guiWidgetTree();
  if (auto* label = dynamic_cast<GuiLabel*>(tree.findWidget(title_label_))) {
    label->text = title_text_;
  }
  if (auto* bar =
          dynamic_cast<EditorToolbarWidget*>(tree.findWidget(toolbar_id_))) {
    bar->setProjectName(loaded ? name : "No project");
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
  bar->setStatusText(formatStatus(*viewport));
  bar->setActiveTool(state_.active_tool);
  bar->tick(tree);
}

bool SimplishEditor::onTick(float /*dt*/) {
  // Re-layout only when the window actually changed size; the chrome is
  // placed manually, so an unconditional pass would be wasted work.
  if (guiLayoutWidth() != laid_out_width_ ||
      guiLayoutHeight() != laid_out_height_) {
    layoutChrome();
  }
  refreshToolbar();
  return true;
}

void SimplishEditor::onClientKeyDown(uint32_t key, ClientKeyDownKind kind) {
  if (kind == ClientKeyDownKind::REPEAT) {
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
  GuiWidgetTree& tree = guiWidgetTree();
  if (auto* bar =
          dynamic_cast<EditorToolbarWidget*>(tree.findWidget(toolbar_id_))) {
    bar->shutdown(tree);
  }
  tree.destroyWidget(toolbar_id_);
  tree.destroyWidget(viewport_id_);
  tree.destroyWidget(title_panel_);
  toolbar_id_ = GUI_WIDGET_ID_INVALID;
  viewport_id_ = GUI_WIDGET_ID_INVALID;
  title_panel_ = GUI_WIDGET_ID_INVALID;
  title_label_ = GUI_WIDGET_ID_INVALID;

  // Release the GuiContext last: the widgets above live in its tree.
  eng::client::RenderedGameClient::onShutdown();
}

}  // namespace eng::editor
