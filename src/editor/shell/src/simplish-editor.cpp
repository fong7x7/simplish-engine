#include <algorithm>
#include <cmath>
#include <editor/project/project-ops.h>
#include <editor/project/project-paths.h>
#include <editor/shell/editor-asset-scan.h>
#include <editor/shell/editor-placement-transform.h>
#include <editor/shell/iso-view-matrix.h>
#include <editor/shell/simplish-editor.h>
#include <engine/core/logger.h>
#include <engine/gui/gui-label.h>
#include <engine/gui/gui-panel.h>
#include <engine/gui/gui-theme-constants.h>
#include <engine/gui/gui-widget-tree.h>
#include <engine/render-mesh/mesh-transform.h>
#include <engine/render-mesh/obj-loader.h>
#include <string>
#include <utility>

namespace eng::editor {

namespace {

  constexpr float TITLE_BAR_HEIGHT = 28.0f;
  constexpr float TITLE_INSET = 10.0f;
  /// How long File > About leaves its line in the toolbar status.
  constexpr float ABOUT_SECONDS = 4.0f;

  /// A layout rect as a pixel scissor. The surface is larger than the
  /// layout on a HiDPI display, so the rect has to be scaled, not copied.
  RhiScissor toSurfaceScissor(const Rect& rect, float scale) {
    return {static_cast<int32_t>(rect.x * scale),
            static_cast<int32_t>(rect.y * scale),
            static_cast<uint32_t>(std::max(0.0f, rect.w * scale)),
            static_cast<uint32_t>(std::max(0.0f, rect.h * scale))};
  }

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

void SimplishEditor::reportProjectOpenFailure(ProjectOpenError error) {
  std::string reason(projectOpenErrorMessage(error));
  LOG_ERROR("editor", "Cannot open project: " + reason);
  // The log is invisible to whoever just picked the wrong folder, and
  // picking the wrong folder is the likely mistake.
  showStatusMessage("Cannot open project: " + reason);
}

bool SimplishEditor::openProjectAt(const std::filesystem::path& root) {
  auto result = openProject(root);
  if (!result.ok()) {
    reportProjectOpenFailure(result.error);
    return false;
  }

  state_.project = std::move(result.context);
  recordProjectOpened(isoTimestampNow());
  LOG_INFO(
      "editor",
      std::string("Opened project: ").append(state_.project.metadata.name));
  applyProjectToChrome();
  return true;
}

void SimplishEditor::recordProjectOpened(const std::string& stamp) {
  // A failed stamp is not fatal — a read-only project still opens; the write
  // failure is worth a line in the log but not a refusal to load.
  if (!touchProjectOpened(state_.project, stamp)) {
    LOG_WARN("editor", "Could not update last_opened_at in project.json");
  }
  promoteRecentProject(state_.recent, state_.project, stamp);
  if (!state_.recent_path.empty()) {
    (void)saveRecentProjects(state_.recent, state_.recent_path);
  }
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
  if (rhiDevice() != nullptr && !mesh_renderer_.init(*rhiDevice())) {
    // Not fatal: the editor runs, placements still show their footprints,
    // and only the geometry is missing.
    LOG_WARN("editor", "Backend has no mesh pipeline; assets will not draw");
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

void SimplishEditor::initAssetPanel(GuiWidgetTree& tree) {
  auto panel = std::make_unique<EditorAssetPanelWidget>();
  panel->on_asset_dropped = [this](size_t index, float x, float y) {
    dropAsset(index, x, y);
  };
  asset_panel_id_ = tree.insertExternalWidget(std::move(panel), root_panel_);
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
  initAssetPanel(tree);
}

EditorViewportWidget* SimplishEditor::viewportWidget() {
  return dynamic_cast<EditorViewportWidget*>(
      guiWidgetTree().findWidget(viewport_id_));
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
  layoutViewportAndAssets(tree, window, toolbar_top + TOOLBAR_HEIGHT);
}

void SimplishEditor::layoutViewportAndAssets(GuiWidgetTree& tree,
                                             const Rect& window, float top) {
  // The asset strip takes the bottom; the viewport gets what is left, which
  // may be nothing at all on a very short window.
  const float panel_top = std::max(top, window.h - ASSET_PANEL_HEIGHT);
  if (auto* viewport = tree.findWidget(viewport_id_)) {
    viewport->rect = makeRect(0.0f, top, window.w, panel_top - top);
  }
  if (auto* panel = tree.findWidget(asset_panel_id_)) {
    panel->rect = makeRect(0.0f, panel_top, window.w, window.h - panel_top);
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
  refreshAssets();
}

void SimplishEditor::refreshAssets() {
  // Placements index into the asset list, and a rescan renumbers it, so
  // they go with it. Nothing is persisted yet either way.
  state_.placements.clear();
  state_.assets = state_.project.loaded
                      ? scanEditorAssets(projectAssetsPath(state_.project.root))
                      : std::vector<EditorAsset>{};
  std::vector<std::string> names;
  names.reserve(state_.assets.size());
  for (const auto& asset : state_.assets) {
    names.push_back(asset.name);
  }
  if (auto* panel = dynamic_cast<EditorAssetPanelWidget*>(
          guiWidgetTree().findWidget(asset_panel_id_))) {
    panel->setAssetNames(std::move(names));
  }
  refreshPlacementMarkers();
}

bool SimplishEditor::loadAssetMesh(EditorAsset& asset) {
  auto mesh = loadObjMesh(asset.path);
  if (!mesh.has_value()) {
    LOG_WARN("editor", "Could not load mesh: " + asset.path.string());
    return false;
  }
  orientYUpToZUp(*mesh);
  auto uploaded = mesh_renderer_.upload(*rhiDevice(), *mesh);
  if (!uploaded.has_value()) {
    return false;
  }
  asset.mesh = *uploaded;
  asset.min = mesh->min;
  asset.max = mesh->max;
  return true;
}

bool SimplishEditor::ensureAssetMesh(size_t index) {
  EditorAsset& asset = state_.assets[index];
  if (asset.mesh != MESH_GPU_INVALID) {
    return true;
  }
  // A failed load is remembered, so a bad file is not reparsed on every
  // drop attempt.
  if (asset.load_failed || !mesh_renderer_.ready()) {
    return false;
  }
  asset.load_failed = !loadAssetMesh(asset);
  return !asset.load_failed;
}

void SimplishEditor::dropAsset(size_t index, float x, float y) {
  EditorViewportWidget* viewport = viewportWidget();
  if (viewport == nullptr || index >= state_.assets.size()) {
    return;
  }
  // A drop anywhere but the viewport is not a placement.
  if (!containsPoint(viewport->rect, x, y)) {
    return;
  }
  if (!ensureAssetMesh(index)) {
    return;
  }
  const IsoView view = makeIsoView(viewport->camera, viewport->rect);
  const WorldPoint world = screenToWorld(view, {x, y});
  state_.placements.push_back(
      {index, {std::floor(world.x), std::floor(world.y)}});
  refreshPlacementMarkers();
}

void SimplishEditor::refreshPlacementMarkers() {
  EditorViewportWidget* viewport = viewportWidget();
  if (viewport == nullptr) {
    return;
  }
  viewport->placement_markers.clear();
  viewport->placement_markers.reserve(state_.placements.size());
  for (const auto& placement : state_.placements) {
    viewport->placement_markers.push_back(placement.position);
  }
}

void SimplishEditor::buildSceneInstances() {
  scene_instances_.clear();
  for (const auto& placement : state_.placements) {
    if (placement.asset >= state_.assets.size()) {
      continue;
    }
    const EditorAsset& asset = state_.assets[placement.asset];
    if (asset.mesh == MESH_GPU_INVALID) {
      continue;
    }
    scene_instances_.push_back(
        {asset.mesh, makePlacementTransform(asset, placement.position)});
  }
}

RhiTextureHandle SimplishEditor::sceneDepthTarget() {
  eng::RhiDevice* device = rhiDevice();
  // No placements means no scene pass at all, which leaves the frame
  // exactly as it was before any of this existed.
  if (device == nullptr || state_.placements.empty()) {
    return RHI_TEXTURE_INVALID;
  }
  return mesh_renderer_.depthTarget(*device, backbufferWidth(),
                                    backbufferHeight());
}

RhiViewport SimplishEditor::surfaceViewport() {
  return {0.0f,
          0.0f,
          static_cast<float>(backbufferWidth()),
          static_cast<float>(backbufferHeight()),
          0.0f,
          1.0f};
}

MeshRenderer::DrawParams
SimplishEditor::sceneDrawParams(const EditorViewportWidget& viewport) {
  const auto width = static_cast<float>(guiLayoutWidth());
  const auto height = static_cast<float>(guiLayoutHeight());
  // The projection maps into full-layout clip space, so the GPU viewport is
  // the whole surface and the scissor is what confines meshes to the
  // editor's viewport rect.
  MeshRenderer::DrawParams params{};
  params.view_projection =
      makeIsoViewProjection(makeIsoView(viewport.camera, viewport.rect),
                            {viewport.rect, width, height});
  params.instances = scene_instances_;
  params.viewport = surfaceViewport();
  params.scissor = toSurfaceScissor(
      viewport.rect,
      width > 0.0f ? static_cast<float>(backbufferWidth()) / width : 1.0f);
  return params;
}

void SimplishEditor::recordScene(RhiCommandList& cmd) {
  EditorViewportWidget* viewport = viewportWidget();
  if (viewport == nullptr) {
    return;
  }
  buildSceneInstances();
  mesh_renderer_.draw(cmd, sceneDrawParams(*viewport));
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

bool SimplishEditor::runDialogCommand(EditorMenuCommand command) {
  // Both answer later, on the main thread, through the on*Chosen overrides.
  if (command == EditorMenuCommand::NEW_PROJECT) {
    showSaveLocationDialog();
    return true;
  }
  if (command == EditorMenuCommand::OPEN_PROJECT) {
    showOpenFolderDialog();
    return true;
  }
  return false;
}

bool SimplishEditor::runProjectCommand(EditorMenuCommand command) {
  if (runDialogCommand(command)) {
    return true;
  }
  if (command == EditorMenuCommand::CLOSE_PROJECT) {
    closeProject();
    return true;
  }
  if (command == EditorMenuCommand::EXIT) {
    quit_requested_ = true;
    return true;
  }
  return false;
}

void SimplishEditor::executeCommand(EditorMenuCommand command) {
  if (runProjectCommand(command)) {
    return;
  }
  if (command == EditorMenuCommand::ABOUT) {
    showAbout();
    return;
  }
  applyViewCommand(command);
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

void SimplishEditor::onSaveLocationChosen(const std::filesystem::path& path) {
  (void)createProjectAt(path);
}

void SimplishEditor::onFolderChosen(const std::filesystem::path& path) {
  // openProjectAt reports its own failure, and leaves any open project
  // alone when the folder turns out not to be one.
  (void)openProjectAt(path);
}

bool SimplishEditor::createProjectAt(const std::filesystem::path& root) {
  // The dialog hands back the full path the user typed, so its last
  // component is the name they chose.
  std::string name = root.filename().string();
  if (name.empty()) {
    name = "Untitled";
  }
  auto result = createProject(root, name, isoTimestampNow());
  if (!result.ok()) {
    LOG_ERROR("editor", std::string("Cannot create project: ")
                            .append(projectOpenErrorMessage(result.error)));
    return false;
  }
  LOG_INFO("editor", "Created project: " + name);
  return openProjectAt(root);
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

void SimplishEditor::showStatusMessage(std::string text) {
  // There is no notification system yet, so messages borrow the toolbar's
  // status line rather than pretending to open a window.
  status_override_ = std::move(text);
  status_override_left_ = ABOUT_SECONDS;
}

void SimplishEditor::showAbout() {
  showStatusMessage("Simplish Editor — engine and editor, C++20");
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
  if (rhiDevice() != nullptr) {
    mesh_renderer_.shutdown(*rhiDevice());
  }
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
  tree.destroyWidget(asset_panel_id_);
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
  asset_panel_id_ = GUI_WIDGET_ID_INVALID;
}

}  // namespace eng::editor
