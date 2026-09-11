#include <algorithm>
#include <cmath>
#include <editor/project/project-ops.h>
#include <editor/project/project-paths.h>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-actor-placement.h>
#include <editor/shell/editor-asset-scan.h>
#include <editor/shell/editor-asset-thumbnail.h>
#include <editor/shell/editor-asset-tree.h>
#include <editor/shell/editor-behavior-choices.h>
#include <editor/shell/editor-character-choices.h>
#include <editor/shell/editor-entity-id.h>
#include <editor/shell/editor-general-section.h>
#include <editor/shell/editor-level-io.h>
#include <editor/shell/editor-level-list.h>
#include <editor/shell/editor-level-ops.h>
#include <editor/shell/editor-light-ops.h>
#include <editor/shell/editor-mesh-style.h>
#include <editor/shell/editor-placement-clip.h>
#include <editor/shell/editor-placement-transform.h>
#include <editor/shell/editor-player-start-ops.h>
#include <editor/shell/editor-project-title.h>
#include <editor/shell/editor-property-ops.h>
#include <editor/shell/editor-shape.h>
#include <editor/shell/editor-thumbnail-cache.h>
#include <editor/shell/editor-waypoint-ops.h>
#include <editor/shell/iso-view-matrix.h>
#include <editor/shell/simplish-editor.h>
#include <engine/client/desktop-platform-keycode.h>
#include <engine/core/logger.h>
#include <engine/gltf/gltf-loader.h>
#include <engine/gltf/skinned-model-orientation.h>
#include <engine/gui/gui-label.h>
#include <engine/gui/gui-panel.h>
#include <engine/gui/gui-theme-constants.h>
#include <engine/gui/gui-widget-tree.h>
#include <engine/gui/image-loader.h>
#include <engine/render-mesh/mesh-transform.h>
#include <engine/render-mesh/obj-loader.h>
#include <engine/render-mesh/skinned-mesh-posing.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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

  /// Whether two placements sit, face, collide, animate and behave exactly
  /// the same way.
  ///
  /// A gesture that ended where it began is not an edit, and an undo entry
  /// that changes nothing is worse than no entry at all. Exact comparison
  /// is right here: the values come from the same arithmetic on both sides,
  /// and a tolerance would swallow the smallest nudge the panel can make.
  bool sameTransform(const EditorPlacement& a, const EditorPlacement& b) {
    return a.position.x == b.position.x && a.position.y == b.position.y &&
           a.position.z == b.position.z && a.rotation.x == b.rotation.x &&
           a.rotation.y == b.rotation.y && a.rotation.z == b.rotation.z &&
           a.scale == b.scale && a.collides == b.collides &&
           a.animation == b.animation && a.behavior == b.behavior &&
           a.faction == b.faction && a.route == b.route;
  }

  /// Whether two lights shine exactly alike, for the same reason
  /// `sameTransform` compares placements exactly.
  bool sameLight(const EditorLight& a, const EditorLight& b) {
    return a.kind == b.kind && a.position.x == b.position.x &&
           a.position.y == b.position.y && a.position.z == b.position.z &&
           a.direction.x == b.direction.x && a.direction.y == b.direction.y &&
           a.direction.z == b.direction.z && a.color.x == b.color.x &&
           a.color.y == b.color.y && a.color.z == b.color.z &&
           a.intensity == b.intensity && a.range == b.range;
  }

  /// Whether two player starts are for the same player, on the same spot,
  /// wearing the same character, for the reason `sameTransform` compares
  /// placements exactly.
  bool sameStart(const EditorPlayerStart& a, const EditorPlayerStart& b) {
    return a.player == b.player && a.position.x == b.position.x &&
           a.position.y == b.position.y && a.position.z == b.position.z &&
           a.character == b.character;
  }

  /// The entry a viewport marker stands for. Markers are the placements,
  /// then the lights, then the player starts — the order
  /// `refreshPlacementMarkers` pushes them in.
  EditorSelection markerSelection(const EditorDocument& document,
                                  size_t marker) {
    const size_t placements = document.placements.size();
    if (marker < placements) {
      return {EditorSelectionKind::PLACEMENT, marker};
    }
    const size_t light = marker - placements;
    if (light < document.lights.size()) {
      return {EditorSelectionKind::LIGHT, light};
    }
    const size_t start = light - document.lights.size();
    if (start < document.player_starts.size()) {
      return {EditorSelectionKind::PLAYER_START, start};
    }
    return {EditorSelectionKind::WAYPOINT,
            start - document.player_starts.size()};
  }

  /// Whether two waypoints stand at the same place in the same route, for
  /// the reason `sameTransform` compares placements exactly.
  bool sameWaypoint(const EditorWaypoint& a, const EditorWaypoint& b) {
    return a.route == b.route && a.order == b.order &&
           a.position.x == b.position.x && a.position.y == b.position.y &&
           a.position.z == b.position.z;
  }

  /// Stands in for an asset a placement names but the list no longer has.
  ///
  /// A reference to this rather than a copy of the real one: markers are
  /// rebuilt on every frame of a property drag, and an EditorAsset carries
  /// two paths and a name.
  const EditorAsset UNKNOWN_ASSET{};

  /// A rigged model's first clip at its first frame — what the viewport
  /// shows the moment it is dropped — as static geometry, for a thumbnail.
  std::optional<MeshData> readRiggedMesh(const std::filesystem::path& path) {
    auto model = gltf::loadGltfModel(path);
    if (!model.has_value()) {
      return std::nullopt;
    }
    gltf::orientSkinnedYUpToZUp(*model);
    animation::RigPose pose;
    return poseSkinnedMesh(model->mesh, pose.evaluate(model->rig, 0, 0.0f));
  }

  /// The geometry an asset stands for: a built-in shape, generated, or the
  /// file it names, read and turned into the world's axes.
  std::optional<MeshData> readAssetMesh(const EditorAsset& asset) {
    if (asset.shape.has_value()) {
      return makeEditorShapeMesh(*asset.shape);
    }
    if (isRiggedModelFile(asset.path)) {
      return readRiggedMesh(asset.path);
    }
    std::optional<MeshData> mesh = loadObjMesh(asset.path);
    if (mesh.has_value()) {
      orientYUpToZUp(*mesh);
    }
    return mesh;
  }

  /// How an image is described to the device as a mesh's diffuse map.
  ///
  /// Unorm rather than sRGB, because the mesh shader converts on the way
  /// out — see `MeshRenderer`. A texture the GPU decoded on sample would be
  /// converted twice and come out washed.
  RhiTextureDesc meshTextureDesc(const ImageData& image) {
    RhiTextureDesc desc{};
    desc.width = image.width;
    desc.height = image.height;
    desc.format = RhiFormat::RGB_A8_UNORM;
    desc.usage = RhiTextureUsage::SAMPLED;
    desc.debug_name = "mesh-texture";
    desc.initial_pixels = image.pixels.data();
    return desc;
  }

  /// Card pictures made per frame.
  ///
  /// Making one means parsing a mesh on a cache miss, which is far too slow
  /// to do for a whole folder at once. A handful a frame fills a screenful
  /// of cards within a few frames of scrolling to them, and never holds a
  /// frame up long enough to be felt.
  constexpr size_t THUMBNAILS_PER_FRAME = 2;

}  // namespace

void SimplishEditor::setRecentProjectsPath(std::filesystem::path path) {
  state_.recent_path = std::move(path);
  if (state_.recent_path.empty()) {
    return;
  }
  // Read here rather than in onInit, which run() calls. A project named on
  // the command line is opened between init() and run(), and opening one
  // promotes it into this list and saves the result — so loading any later
  // than this means saving over the file before ever having read it, and
  // every launch with a project argument would drop the history.
  state_.recent = loadRecentProjects(state_.recent_path);
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
  initSceneRenderers();
  initChrome();
  applyProjectToChrome();
  layoutChrome();
  return true;
}

void SimplishEditor::initSceneRenderers() {
  RhiDevice* device = rhiDevice();
  if (device == nullptr) {
    return;
  }
  if (!mesh_renderer_.init(*device)) {
    // Not fatal: the editor runs, placements still show their footprints,
    // and only the geometry is missing.
    LOG_WARN("editor", "Backend has no mesh pipeline; assets will not draw");
    return;
  }
  if (!outline_renderer_.init(*device)) {
    // Cel shading still bands the light; it only loses its line.
    LOG_WARN("editor", "Backend has no outline pipeline; cel shading will "
                       "draw without outlines");
  }
  if (!skinned_renderer_.init(*device)) {
    LOG_WARN("editor", "Backend has no skinned mesh pipeline; rigged models "
                       "will not load");
  }
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
    // Transparent: the chrome tiles the whole window with its own opaque
    // panels, and an opaque root would paint over the scene pass.
    panel->fill_color = GuiColor{0, 0, 0, 0};
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
  menu_bar->on_open_level = [this](std::string_view id) {
    // REFUSE: a level row is one click away from a level's worth of work,
    // and the status line is a cheaper answer than losing it.
    openLevel(id, EditorLevelUnsaved::REFUSE);
  };
  menu_bar_id_ = tree.insertExternalWidget(std::move(menu_bar), root_panel_);
  if (auto* bar =
          dynamic_cast<EditorMenuBarWidget*>(tree.findWidget(menu_bar_id_))) {
    bar->init(tree);
  }
}

void SimplishEditor::initAssetPanel(GuiWidgetTree& tree) {
  auto panel = std::make_unique<EditorAssetBrowserWidget>();
  panel->on_asset_dropped = [this](size_t entry, float x, float y) {
    dropBrowserEntry(entry, x, y);
  };
  asset_panel_id_ = tree.insertExternalWidget(std::move(panel), root_panel_);
}

void SimplishEditor::initPropertiesPanel(GuiWidgetTree& tree) {
  auto panel = std::make_unique<EditorPropertiesWidget>();
  panel->on_property_changed = [this](EditorPropertyField field, float value,
                                      EditorPropertyEdit edit) {
    applyPropertyEdit(field, value, edit);
  };
  panel->on_choice_changed = [this](EditorChoiceKind kind, size_t index) {
    applyChoiceEdit(kind, index);
  };
  properties_panel_id_ =
      tree.insertExternalWidget(std::move(panel), root_panel_);
}

void SimplishEditor::initToolbar(GuiWidgetTree& tree) {
  auto toolbar = std::make_unique<EditorToolbarWidget>();
  toolbar->on_tool_selected = [this](EditorTool tool) {
    state_.active_tool = tool;
  };
  toolbar->on_play_toggled = [this] {
    togglePlaytest();
  };
  toolbar_id_ = tree.insertExternalWidget(std::move(toolbar), root_panel_);
  if (auto* bar =
          dynamic_cast<EditorToolbarWidget*>(tree.findWidget(toolbar_id_))) {
    bar->init(tree);
  }
}

void SimplishEditor::initWorkArea(GuiWidgetTree& tree) {
  initToolbar(tree);
  auto viewport = std::make_unique<EditorViewportWidget>();
  viewport->on_placement_picked = [this](int marker) {
    selectMarker(marker);
  };
  viewport_id_ = tree.insertExternalWidget(std::move(viewport), root_panel_);
  initPropertiesPanel(tree);
  initAssetPanel(tree);
  // Last, so it draws over and is hit before everything it covers.
  initCharacterSelect(tree);
}

void SimplishEditor::initCharacterSelect(GuiWidgetTree& tree) {
  auto select = std::make_unique<EditorCharacterSelectWidget>();
  select->on_chosen = [this](size_t index) {
    const auto& characters = state_.characters.characters;
    if (index < characters.size()) {
      startPlaytestAs(characters[index].id);
    }
  };
  select->on_cancelled = [this] {
    closeCharacterSelect();
  };
  character_select_id_ =
      tree.insertExternalWidget(std::move(select), root_panel_);
}

EditorViewportWidget* SimplishEditor::viewportWidget() {
  return dynamic_cast<EditorViewportWidget*>(
      guiWidgetTree().findWidget(viewport_id_));
}

EditorAssetBrowserWidget* SimplishEditor::assetBrowserWidget() {
  return dynamic_cast<EditorAssetBrowserWidget*>(
      guiWidgetTree().findWidget(asset_panel_id_));
}

EditorPropertiesWidget* SimplishEditor::propertiesWidget() {
  return dynamic_cast<EditorPropertiesWidget*>(
      guiWidgetTree().findWidget(properties_panel_id_));
}

float SimplishEditor::propertiesPanelWidth() {
  const EditorPropertiesWidget* panel = propertiesWidget();
  return panel != nullptr ? panel->preferredWidth() : 0.0f;
}

float SimplishEditor::assetBrowserHeight() {
  const EditorAssetBrowserWidget* panel = assetBrowserWidget();
  return panel != nullptr ? panel->preferredHeight() : ASSET_PANEL_HEIGHT;
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
  laid_out_panel_height_ = assetBrowserHeight();
  laid_out_properties_width_ = propertiesPanelWidth();
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
  // The asset browser takes the bottom; the viewport gets what is left,
  // which may be nothing at all on a very short window. Folding the browser
  // hands most of that back.
  const float panel_top = std::max(top, window.h - assetBrowserHeight());
  // The properties panel takes the right of what is left, and only while
  // something is selected; the viewport gets the rest.
  const float properties_w = std::min(propertiesPanelWidth(), window.w);
  const float viewport_w = window.w - properties_w;
  if (auto* viewport = tree.findWidget(viewport_id_)) {
    viewport->rect = makeRect(0.0f, top, viewport_w, panel_top - top);
  }
  if (auto* panel = tree.findWidget(properties_panel_id_)) {
    panel->rect = makeRect(viewport_w, top, properties_w, panel_top - top);
  }
  if (auto* panel = tree.findWidget(asset_panel_id_)) {
    panel->rect = makeRect(0.0f, panel_top, window.w, window.h - panel_top);
  }
  // Over the viewport and nothing else: the selector is about the level
  // being played, and the panels around it stay where they are.
  if (auto* select = tree.findWidget(character_select_id_)) {
    select->rect = makeRect(0.0f, top, viewport_w, panel_top - top);
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
  applyProjectNameToChrome();
  applyProjectToWidgets();
  applyProjectionToWidgets();
  applyShadingToWidgets();
}

void SimplishEditor::applyProjectNameToChrome() {
  shown_unsaved_ = hasUnsavedEditorChanges(state_.history);
  title_text_ = editorProjectTitle(state_);
  setWindowTitle(title_text_);
  GuiWidgetTree& tree = guiWidgetTree();
  if (auto* label = dynamic_cast<GuiLabel*>(tree.findWidget(title_label_))) {
    label->text = title_text_;
  }
  if (auto* bar =
          dynamic_cast<EditorToolbarWidget*>(tree.findWidget(toolbar_id_))) {
    bar->setProjectName(editorProjectDisplayName(state_));
  }
}

void SimplishEditor::refreshUnsavedMarker() {
  if (toolbar_id_ == GUI_WIDGET_ID_INVALID ||
      hasUnsavedEditorChanges(state_.history) == shown_unsaved_) {
    return;
  }
  // The name and nothing else. `applyProjectToChrome` reloads the project's
  // assets, which drops the document and reads the level back from disk —
  // out from under the very edit that just set this flag. And only on the
  // transition: this runs after every edit, and a property drag is an edit
  // a frame.
  applyProjectNameToChrome();
}

void SimplishEditor::applyProjectToWidgets() {
  const bool loaded = state_.project.loaded;
  GuiWidgetTree& tree = guiWidgetTree();
  if (auto* menu =
          dynamic_cast<EditorMenuBarWidget*>(tree.findWidget(menu_bar_id_))) {
    menu->setProjectPresence(loaded ? EditorProjectPresence::OPEN
                                    : EditorProjectPresence::NONE);
    menu->setRecentProjects(state_.recent);
  }
  refreshAssets();
}

void SimplishEditor::clearDocument() {
  state_.document = EditorDocument{};
  state_.selection = EditorSelection{};
  placement_prior_.reset();
  light_prior_.reset();
  player_start_prior_.reset();
  clearEditorActions(state_.history);
}

void SimplishEditor::adoptAssetScan(EditorAssetScan scan) {
  state_.asset_tree = buildEditorAssetTree(scan);
  state_.assets = std::move(scan.assets);
  // The built-in shapes go on the end of the asset list, so a placement
  // names one exactly as it names a scanned model; the lights are numbered
  // past every asset. That numbering is what the browser reports a drop in.
  const size_t first_shape = appendEditorShapeAssets(state_.assets);
  appendEditorGeneralSection(state_.asset_tree, first_shape,
                             state_.assets.size());
  // Once, here, with the list complete: an id has to be unique across
  // every asset, and the built-in shapes are assets like any other.
  assignEditorAssetIds(state_.assets);
}

void SimplishEditor::reloadAssets() {
  // The textures belong to the list about to be replaced, and nothing else
  // will ever hold their handles again.
  releaseAssetThumbnails();
  releaseAssetTextures();
  adoptAssetScan(state_.project.loaded
                     ? scanEditorAssets(projectAssetsPath(state_.project.root))
                     : EditorAssetScan{});
  reloadDataTables();
  refreshAssetPanel();
}

void SimplishEditor::refreshAssets() {
  // A different project, so the document goes with the old one: the lights
  // name no asset, but they belong to the level being closed, and leaving
  // them behind would light the next project with them. A playtest of it
  // goes first, for the same reason.
  stopPlaytest();
  clearDocument();
  reloadAssets();
  // Which level, before what is in it: the previous project's level id is
  // still in the state until this runs.
  chooseEditorStartLevel(state_);
  loadDocument();
  // What is in memory is what is on disk, whether that was read from a
  // level file or is the empty document a project without one opens at.
  markEditorChangesSaved(state_.history);
  applyEditToChrome();
}

bool SimplishEditor::canSaveDocument() {
  if (!state_.project.loaded) {
    return false;
  }
  // The editor cannot show a level it could not parse, so what it would
  // write here is an empty one over whatever the file actually holds.
  if (!state_.level_readable) {
    showStatusMessage("Not saving over a level file that could not be read");
    return false;
  }
  return true;
}

bool SimplishEditor::writeLevelFile() {
  if (saveEditorLevel(state_)) {
    return true;
  }
  LOG_ERROR("editor", "Could not write " + editorLevelPath(state_).string());
  showStatusMessage("Could not save the level");
  return false;
}

void SimplishEditor::saveDocument() {
  if (!canSaveDocument()) {
    return;
  }
  // A property drag still in flight has already changed the document, so
  // recording it first is what makes the file on disk the level on screen.
  commitPendingEdit();
  if (!writeLevelFile()) {
    return;
  }
  markEditorChangesSaved(state_.history);
  // The name only, for the reason `refreshUnsavedMarker` gives: the whole
  // chrome would re-read the level that was just written and drop the undo
  // history describing it.
  applyProjectNameToChrome();
  LOG_INFO("editor", "Saved level: " + editorLevelPath(state_).string());
  showStatusMessage("Saved " + state_.project.metadata.name);
}

void SimplishEditor::createLevel(std::string_view id,
                                 EditorLevelUnsaved unsaved) {
  stopPlaytest();
  commitPendingEdit();
  applyLevelResult(createEditorLevel(state_, id, unsaved), id);
}

void SimplishEditor::openLevel(std::string_view id,
                               EditorLevelUnsaved unsaved) {
  stopPlaytest();
  commitPendingEdit();
  applyLevelResult(openEditorLevel(state_, id, unsaved), id);
}

void SimplishEditor::applyLevelResult(const EditorLevelResult& result,
                                      std::string_view id) {
  // UNREADABLE as well as OK: the level was switched to and its file could
  // not be parsed, so the chrome has to show the empty document that left
  // behind rather than the previous level's.
  if (result.status == EditorLevelStatus::OK ||
      result.status == EditorLevelStatus::UNREADABLE) {
    adoptOpenedLevel(result);
    LOG_INFO("editor", "Editing level " + editorLevelPath(state_).string());
  }
  showStatusMessage(result.status == EditorLevelStatus::OK
                        ? "Editing level " + std::string(id)
                        : std::string(editorLevelStatusMessage(result.status)));
}

void SimplishEditor::adoptOpenedLevel(const EditorLevelResult& result) {
  // A gesture in flight belongs to a document that is no longer here.
  placement_prior_.reset();
  light_prior_.reset();
  player_start_prior_.reset();
  reportDroppedProps(result.dropped_props);
  reportDroppedEntities(result.dropped_entities);
  ensurePlacedMeshes();
  applyEditToChrome();
  applyProjectNameToChrome();
}

void SimplishEditor::loadDocument() {
  state_.level_readable = true;
  // No file is the ordinary state of a project nothing has been saved into,
  // and it is not something to report as a failure.
  if (!editorLevelExists(state_)) {
    return;
  }
  std::optional<EditorLevelLoad> load = loadEditorLevel(state_);
  if (!load) {
    reportLevelUnreadable();
    return;
  }
  reportDroppedProps(load->dropped_props);
  reportDroppedEntities(load->dropped_entities);
  state_.document = std::move(load->document);
  ensurePlacedMeshes();
}

void SimplishEditor::reportLevelUnreadable() {
  // Remembered, not just logged: this is what stops the next save from
  // writing an empty level over the file that could not be parsed.
  state_.level_readable = false;
  LOG_ERROR("editor", "Could not read " + editorLevelPath(state_).string());
  showStatusMessage("Could not read the project's level");
}

void SimplishEditor::reportDroppedProps(size_t dropped) {
  if (dropped == 0) {
    return;
  }
  LOG_WARN("editor", "Level dropped " + std::to_string(dropped) +
                         " prop(s) whose asset is gone");
}

void SimplishEditor::reportDroppedEntities(size_t dropped) {
  if (dropped == 0) {
    return;
  }
  LOG_WARN("editor", "Level dropped " + std::to_string(dropped) +
                         " entit(ies) this editor has no definition for");
}

void SimplishEditor::refreshAssetPanel() {
  std::vector<std::string> names;
  names.reserve(state_.assets.size() + EDITOR_GENERAL_ITEM_COUNT);
  for (const EditorAsset& asset : state_.assets) {
    names.push_back(asset.name);
  }
  // Named after the assets, in the order the general section numbers them,
  // because that is the numbering its folder holds.
  for (const EditorGeneralItem item : EDITOR_GENERAL_ITEMS) {
    names.emplace_back(editorGeneralItemName(item));
  }
  if (auto* panel = dynamic_cast<EditorAssetBrowserWidget*>(
          guiWidgetTree().findWidget(asset_panel_id_))) {
    panel->setAssets(state_.asset_tree, std::move(names));
  }
}

RhiTextureHandle
SimplishEditor::uploadMeshTexture(const std::filesystem::path& path) {
  RhiDevice* device = rhiDevice();
  if (device == nullptr || path.empty()) {
    return RHI_TEXTURE_INVALID;
  }
  const std::optional<ImageData> image =
      ImageLoader::loadFromFile(path.string());
  if (!image.has_value() || image->pixels.empty()) {
    // Not fatal: the model still draws, in the flat colour an untextured
    // one takes. A missing map is a project problem, not an editor one.
    LOG_WARN("editor", "Could not load texture: " + path.string());
    return RHI_TEXTURE_INVALID;
  }
  return device->createTexture(meshTextureDesc(*image));
}

bool SimplishEditor::adoptRiggedModel(EditorAsset& asset,
                                      gltf::SkinnedModel& model) {
  gltf::orientSkinnedYUpToZUp(model);
  auto uploaded = skinned_renderer_.upload(*rhiDevice(), model.mesh);
  if (!uploaded.has_value()) {
    return false;
  }
  asset.skinned_mesh = *uploaded;
  asset.min = model.mesh.min;
  asset.max = model.mesh.max;
  asset.texture = uploadMeshTexture(model.mesh.texture_path);
  asset.rig = std::make_shared<const animation::Rig>(std::move(model.rig));
  return true;
}

bool SimplishEditor::loadRiggedAsset(EditorAsset& asset) {
  auto model = gltf::loadGltfModel(asset.path);
  if (model.has_value() && skinned_renderer_.ready()) {
    return adoptRiggedModel(asset, *model);
  }
  const std::string reason =
      model.has_value() ? "this backend cannot draw rigged models"
                        : std::string(gltfLoadErrorMessage(model.error()));
  showStatusMessage("Cannot load " + asset.name + ": " + reason);
  return false;
}

bool SimplishEditor::loadAssetMesh(EditorAsset& asset) {
  std::optional<MeshData> mesh = readAssetMesh(asset);
  if (!mesh.has_value()) {
    LOG_WARN("editor", "Could not load mesh: " + asset.path.string());
    return false;
  }
  auto uploaded = mesh_renderer_.upload(*rhiDevice(), *mesh);
  if (!uploaded.has_value()) {
    return false;
  }
  asset.mesh = *uploaded;
  asset.min = mesh->min;
  asset.max = mesh->max;
  asset.texture = uploadMeshTexture(mesh->texture_path);
  return true;
}

void SimplishEditor::releaseAssetTextures() {
  RhiDevice* device = rhiDevice();
  for (EditorAsset& asset : state_.assets) {
    if (device != nullptr && asset.texture != RHI_TEXTURE_INVALID) {
      device->destroyTexture(asset.texture);
    }
    asset.texture = RHI_TEXTURE_INVALID;
  }
}

void SimplishEditor::releaseAssetThumbnails() {
  RhiDevice* device = rhiDevice();
  for (EditorAsset& asset : state_.assets) {
    if (device != nullptr && asset.thumbnail != RHI_TEXTURE_INVALID) {
      device->destroyTexture(asset.thumbnail);
    }
    asset.thumbnail = RHI_TEXTURE_INVALID;
    asset.thumbnail_state = EditorAssetThumbnailState::PENDING;
  }
}

std::filesystem::path SimplishEditor::thumbnailCacheDir() const {
  // A subdirectory per projection, because a card is drawn at the angle the
  // viewport uses: switching projection has to miss the cache rather than
  // show pictures of the old angle, and switching back should still hit.
  return projectThumbnailsPath(state_.project.root) /
         projectProjectionName(state_.project.metadata.projection);
}

ImageData SimplishEditor::buildAssetThumbnail(const EditorAsset& asset) {
  // A shape's geometry costs a few hundred triangles of trigonometry to
  // rebuild, and there is no file to key a cache entry on.
  if (!asset.shape.has_value()) {
    return buildCachedThumbnail(asset);
  }
  return renderAssetThumbnail(makeEditorShapeMesh(*asset.shape),
                              ASSET_THUMBNAIL_SIZE, thumbnailAxes());
}

IsoAxes SimplishEditor::thumbnailAxes() const {
  return isoAxesFor(state_.project.metadata.projection);
}

ImageData SimplishEditor::buildCachedThumbnail(const EditorAsset& asset) {
  const ThumbnailCacheEntry entry{thumbnailCacheDir(), asset.path,
                                  asset.relative_path};
  if (std::optional<ImageData> cached = loadCachedThumbnail(entry)) {
    return std::move(*cached);
  }
  // The expensive half, and the reason the cache exists: parsing a mesh the
  // editor may never otherwise need to read.
  const std::optional<MeshData> mesh = readAssetMesh(asset);
  if (!mesh.has_value()) {
    return {};
  }
  ImageData image =
      renderAssetThumbnail(*mesh, ASSET_THUMBNAIL_SIZE, thumbnailAxes());
  storeCachedThumbnail(entry, image);
  return image;
}

bool SimplishEditor::uploadAssetThumbnail(EditorAsset& asset,
                                          const ImageData& image) {
  RhiDevice* device = rhiDevice();
  if (device == nullptr || image.pixels.empty()) {
    return false;
  }
  RhiTextureDesc desc{};
  desc.width = image.width;
  desc.height = image.height;
  desc.format = RhiFormat::RGB_A8_SRGB;
  desc.usage = RhiTextureUsage::SAMPLED;
  desc.debug_name = "asset-thumbnail";
  desc.initial_pixels = image.pixels.data();
  asset.thumbnail = device->createTexture(desc);
  return asset.thumbnail != RHI_TEXTURE_INVALID;
}

bool SimplishEditor::ensureAssetThumbnail(size_t index) {
  EditorAsset& asset = state_.assets[index];
  if (asset.thumbnail_state != EditorAssetThumbnailState::PENDING) {
    return false;
  }
  const ImageData image = buildAssetThumbnail(asset);
  const bool ready = uploadAssetThumbnail(asset, image);
  // Failure is remembered either way: a mesh that will not parse would
  // otherwise be reparsed for as long as its card is on screen.
  asset.thumbnail_state = ready ? EditorAssetThumbnailState::READY
                                : EditorAssetThumbnailState::FAILED;
  return true;
}

size_t SimplishEditor::pumpThumbnailRange(EditorAssetBrowserWidget& browser,
                                          size_t first, size_t last) {
  const std::vector<size_t>& shown = browser.visibleAssets();
  size_t made = 0;
  for (size_t slot = first; slot < last && made < THUMBNAILS_PER_FRAME;
       ++slot) {
    const size_t asset = shown[slot];
    // A built-in item's card stands for no file, so there is no mesh to
    // make a picture of; its card shows the empty well instead.
    if (asset >= state_.assets.size()) {
      continue;
    }
    if (ensureAssetThumbnail(asset)) {
      browser.setAssetThumbnail(asset, state_.assets[asset].thumbnail);
      ++made;
    }
  }
  return made;
}

void SimplishEditor::pumpThumbnails() {
  auto* browser = assetBrowserWidget();
  if (browser == nullptr) {
    return;
  }
  const size_t first = browser->firstVisibleSlot();
  const size_t last = std::min(first + browser->visibleSlotCount(),
                               browser->visibleAssets().size());
  pumpThumbnailRange(*browser, first, last);
}

bool SimplishEditor::ensureAssetMesh(size_t index) {
  EditorAsset& asset = state_.assets[index];
  if (editorAssetLoaded(asset)) {
    return true;
  }
  // A failed load is remembered, so a bad file is not reparsed on every
  // drop attempt.
  if (asset.load_failed || !mesh_renderer_.ready()) {
    return false;
  }
  const bool rigged = !asset.shape.has_value() && isRiggedModelFile(asset.path);
  asset.load_failed = !(rigged ? loadRiggedAsset(asset) : loadAssetMesh(asset));
  return !asset.load_failed;
}

void SimplishEditor::dropBrowserEntry(size_t entry, float x, float y) {
  EditorViewportWidget* viewport = viewportWidget();
  // A drop anywhere but the viewport is not a placement, and nothing is
  // placed in a level while it is being played.
  if (viewport == nullptr || isPlaying() ||
      !containsPoint(viewport->rect, x, y)) {
    return;
  }
  const IsoView view = makeIsoView(viewport->camera, viewport->rect);
  const WorldPoint world = screenToWorld(view, {x, y});
  placeBrowserEntry(entry, {std::floor(world.x), std::floor(world.y)});
}

void SimplishEditor::placeBrowserEntry(size_t entry, WorldPoint tile) {
  if (entry < state_.assets.size()) {
    if (ensureAssetMesh(entry)) {
      placeAsset(entry, tile);
    }
    return;
  }
  // Past the assets are the built-in items, in the order the general
  // section lists them. An entry past those is a drop the browser should
  // never have reported.
  const size_t item = entry - state_.assets.size();
  if (item < EDITOR_GENERAL_ITEM_COUNT) {
    placeGeneralItem(EDITOR_GENERAL_ITEMS[item], tile);
  }
}

void SimplishEditor::placeGeneralItem(EditorGeneralItem item, WorldPoint tile) {
  if (const std::optional<EditorLightKind> kind =
          editorGeneralItemLightKind(item)) {
    placeLight(*kind, tile);
  } else if (item == EditorGeneralItem::WAYPOINT) {
    placeWaypoint(tile);
  } else {
    placePlayerStart(tile);
  }
}

void SimplishEditor::placeWaypoint(WorldPoint tile) {
  const uint8_t route =
      editSubjectSelected(EditorSelectionKind::WAYPOINT)
          ? state_.document.waypoints[state_.selection.index].route
          : uint8_t{1};
  const size_t added = state_.document.waypoints.size();
  EditorWaypoint waypoint =
      makeEditorWaypoint(route, nextEditorWaypointOrder(state_.document, route),
                         {tile.x + 0.5f, tile.y + 0.5f, 0.0f});
  waypoint.id = mintEditorWaypointId(state_.document);
  recordAction({.kind = EditorActionKind::ADD_WAYPOINT,
                .index = added,
                .waypoint = waypoint});
  select({EditorSelectionKind::WAYPOINT, added});
}

void SimplishEditor::placeAsset(size_t index, WorldPoint position) {
  // Appended, so undo takes the newest placement off the end and redo puts
  // it back at the same index.
  const size_t placed = state_.document.placements.size();
  EditorPlacement placement{};
  placement.id = mintEditorPlacementId(state_.document, state_.assets[index]);
  placement.asset = index;
  placement.position = position;
  recordAction({.kind = EditorActionKind::PLACE_ASSET,
                .index = placed,
                .placement = placement});
  // Selecting what was just dropped opens the panel on it, which is what
  // somebody who wants it a quarter-tile to the left is about to reach for.
  select({EditorSelectionKind::PLACEMENT, placed});
}

void SimplishEditor::placeLight(EditorLightKind kind, WorldPoint tile) {
  // Over the middle of the tile it was dropped on, and above head height,
  // so a point light lights what is around it rather than sitting inside a
  // prop standing there.
  const WorldPoint position{tile.x + 0.5f, tile.y + 0.5f,
                            EDITOR_LIGHT_DROP_HEIGHT};
  const size_t added = state_.document.lights.size();
  EditorLight light = makeEditorLight(kind, position);
  light.id = mintEditorLightId(state_.document, kind);
  recordAction(
      {.kind = EditorActionKind::ADD_LIGHT, .index = added, .light = light});
  select({EditorSelectionKind::LIGHT, added});
}

void SimplishEditor::placePlayerStart(WorldPoint tile) {
  // Feet on the middle of the tile it was dropped on, where a player
  // standing on that tile would be.
  const WorldPoint position{tile.x + 0.5f, tile.y + 0.5f, 0.0f};
  const size_t added = state_.document.player_starts.size();
  EditorPlayerStart start =
      makeEditorPlayerStart(nextEditorPlayerSlot(state_.document), position);
  start.id = mintEditorPlayerStartId(state_.document);
  recordAction({.kind = EditorActionKind::ADD_PLAYER_START,
                .index = added,
                .player_start = start});
  select({EditorSelectionKind::PLAYER_START, added});
}

size_t SimplishEditor::selectionCount() const {
  // Nothing selected has no list, so no index is ever in range — which is
  // what every caller here asks this in order to find out.
  return editorListSize(state_.document, state_.selection.kind);
}

bool SimplishEditor::isSelected(EditorSelectionKind kind, size_t index) const {
  return selectionIs(state_.selection, kind) && state_.selection.index == index;
}

void SimplishEditor::select(EditorSelection selection) {
  // A selection change ends any edit the panel had in flight, and that edit
  // belongs to what is selected now rather than to what is about to be — so
  // it is recorded before the selection moves off it. Dropping it instead
  // would leave the value the drag reached in the document with no action
  // describing it, and nothing able to undo it.
  commitPendingEdit();
  state_.selection = selection;
  if (state_.selection.index >= selectionCount()) {
    state_.selection = EditorSelection{};
  }
  applySelectionToChrome();
}

void SimplishEditor::selectMarker(int marker) {
  // A click while playing is the fire button, not a pick.
  if (isPlaying()) {
    return;
  }
  if (marker < 0) {
    select({});
    return;
  }
  select(markerSelection(state_.document, static_cast<size_t>(marker)));
}

void SimplishEditor::showPlacementSelection(EditorPropertiesWidget& panel) {
  const EditorPlacement& placement =
      state_.document.placements[state_.selection.index];
  // An asset the list no longer has leaves the line blank rather than
  // naming whatever took its number.
  const std::string name = placement.asset < state_.assets.size()
                               ? state_.assets[placement.asset].name
                               : std::string{};
  panel.setSelection(name, placement);
  if (placement.asset < state_.assets.size()) {
    std::vector<std::string> clips =
        editorClipNames(state_.assets[placement.asset].rig.get());
    const size_t current = editorClipIndex(clips, placement.animation);
    panel.addChoices(EditorChoiceKind::ANIMATION, std::move(clips), current);
  }
  showActorChoices(panel, placement);
}

void SimplishEditor::showActorChoices(EditorPropertiesWidget& panel,
                                      const EditorPlacement& placement) const {
  EditorBehaviorChoices behaviors =
      editorBehaviorChoices(state_.behaviors.behaviors, placement.behavior);
  panel.addChoices(EditorChoiceKind::BEHAVIOR, std::move(behaviors.names),
                   behaviors.current);
  // A faction is a side for an actor to be on, and a route a round for it
  // to walk; scenery has neither to pick.
  if (isEditorActor(placement)) {
    panel.addChoices(EditorChoiceKind::FACTION, editorFactionNames(),
                     static_cast<size_t>(placement.faction));
    EditorRouteChoices routes =
        editorRouteChoices(state_.document, placement.route);
    panel.addChoices(EditorChoiceKind::ROUTE, std::move(routes.names),
                     routes.current);
  }
}

void SimplishEditor::showLightSelection(EditorPropertiesWidget& panel) {
  const EditorLight& light = state_.document.lights[state_.selection.index];
  panel.setSelection(std::string(editorLightKindName(light.kind)), light);
}

void SimplishEditor::showPlayerStartSelection(EditorPropertiesWidget& panel) {
  const EditorPlayerStart& start =
      state_.document.player_starts[state_.selection.index];
  panel.setSelection(editorPlayerStartName(start), start);
  EditorCharacterChoices choices =
      editorCharacterChoices(state_.characters.characters, start.character);
  panel.addChoices(EditorChoiceKind::CHARACTER, std::move(choices.names),
                   choices.current);
}

void SimplishEditor::showWaypointSelection(EditorPropertiesWidget& panel) {
  const EditorWaypoint& waypoint =
      state_.document.waypoints[state_.selection.index];
  panel.setSelection(editorWaypointName(waypoint), waypoint);
}

void SimplishEditor::applySelectionToChrome() {
  auto* panel = propertiesWidget();
  if (panel == nullptr) {
    return;
  }
  if (state_.selection.index >= selectionCount()) {
    panel->clearSelection();
  } else if (selectionIs(state_.selection, EditorSelectionKind::PLACEMENT)) {
    showPlacementSelection(*panel);
  } else if (selectionIs(state_.selection, EditorSelectionKind::LIGHT)) {
    showLightSelection(*panel);
  } else if (selectionIs(state_.selection, EditorSelectionKind::WAYPOINT)) {
    showWaypointSelection(*panel);
  } else {
    showPlayerStartSelection(*panel);
  }
  refreshPlacementMarkers();
}

void SimplishEditor::applyPropertyEdit(EditorPropertyField field, float value,
                                       EditorPropertyEdit edit) {
  if (isPlaying() || state_.selection.index >= selectionCount()) {
    return;
  }
  if (selectionIs(state_.selection, EditorSelectionKind::PLACEMENT)) {
    applyPlacementEdit(field, value, edit);
  } else if (selectionIs(state_.selection, EditorSelectionKind::LIGHT)) {
    applyLightEdit(field, value, edit);
  } else if (selectionIs(state_.selection, EditorSelectionKind::WAYPOINT)) {
    applyWaypointEdit(field, value, edit);
  } else {
    applyPlayerStartEdit(field, value, edit);
  }
  refreshPlacementMarkers();
}

void SimplishEditor::applyPlacementEdit(EditorPropertyField field, float value,
                                        EditorPropertyEdit edit) {
  EditorPlacement& placement =
      state_.document.placements[state_.selection.index];
  // The first change of a gesture is what the eventual undo restores, so
  // the placement is copied before it is written to.
  if (!placement_prior_.has_value()) {
    placement_prior_ = placement;
  }
  setEditorPropertyValue(placement, field, value);
  if (edit == EditorPropertyEdit::COMMIT) {
    commitPlacementEdit();
  }
}

void SimplishEditor::applyChoiceEdit(EditorChoiceKind kind, size_t index) {
  switch (kind) {
    case EditorChoiceKind::ANIMATION:
      applyClipChoice(index);
      break;
    case EditorChoiceKind::CHARACTER:
      applyCharacterChoice(index);
      break;
    case EditorChoiceKind::BEHAVIOR:
    case EditorChoiceKind::FACTION:
    case EditorChoiceKind::ROUTE:
      applyActorChoice(kind, index);
      break;
  }
}

void SimplishEditor::applyActorChoice(EditorChoiceKind kind, size_t index) {
  if (kind == EditorChoiceKind::BEHAVIOR) {
    applyBehaviorChoice(index);
  } else if (kind == EditorChoiceKind::FACTION) {
    applyFactionChoice(index);
  } else {
    applyRouteChoice(index);
  }
}

void SimplishEditor::applyBehaviorChoice(size_t index) {
  if (!editSubjectSelected(EditorSelectionKind::PLACEMENT)) {
    return;
  }
  const EditorPlacement& placement =
      state_.document.placements[state_.selection.index];
  // Worked out again, as the character choices are, rather than
  // remembered from when the panel was shown.
  const EditorBehaviorChoices choices =
      editorBehaviorChoices(state_.behaviors.behaviors, placement.behavior);
  if (index < choices.refs.size()) {
    applyActorEdit(choices.refs[index], placement.faction);
  }
}

void SimplishEditor::applyFactionChoice(size_t index) {
  if (editSubjectSelected(EditorSelectionKind::PLACEMENT) &&
      index < game::ALL_FACTIONS.size()) {
    applyActorEdit(state_.document.placements[state_.selection.index].behavior,
                   game::ALL_FACTIONS[index]);
  }
}

void SimplishEditor::applyRouteChoice(size_t index) {
  if (isPlaying() || !editSubjectSelected(EditorSelectionKind::PLACEMENT)) {
    return;
  }
  EditorPlacement& placement =
      state_.document.placements[state_.selection.index];
  const EditorRouteChoices routes =
      editorRouteChoices(state_.document, placement.route);
  if (index < routes.routes.size()) {
    placement_prior_ = placement;
    placement.route = routes.routes[index];
    commitPlacementEdit();
  }
}

void SimplishEditor::applyActorEdit(const std::string& behavior,
                                    game::Faction faction) {
  if (isPlaying() || !editSubjectSelected(EditorSelectionKind::PLACEMENT)) {
    return;
  }
  EditorPlacement& placement =
      state_.document.placements[state_.selection.index];
  if (!placement_prior_.has_value()) {
    placement_prior_ = placement;
  }
  placement.behavior = behavior;
  placement.faction = faction;
  commitPlacementEdit();
}

void SimplishEditor::applyCharacterChoice(size_t index) {
  if (!editSubjectSelected(EditorSelectionKind::PLAYER_START)) {
    return;
  }
  // Worked out again rather than remembered from when the panel was shown:
  // the choices are a function of the assets and the start, and both are
  // right here.
  const EditorCharacterChoices choices = editorCharacterChoices(
      state_.characters.characters,
      state_.document.player_starts[state_.selection.index].character);
  if (index < choices.refs.size()) {
    applyCharacterEdit(choices.refs[index]);
  }
}

void SimplishEditor::applyClipChoice(size_t index) {
  if (!editSubjectSelected(EditorSelectionKind::PLACEMENT)) {
    return;
  }
  const size_t asset = state_.document.placements[state_.selection.index].asset;
  const std::vector<std::string> clips = editorClipNames(
      asset < state_.assets.size() ? state_.assets[asset].rig.get() : nullptr);
  if (index < clips.size()) {
    applyClipEdit(clips[index]);
  }
}

void SimplishEditor::applyCharacterEdit(const std::string& character) {
  if (isPlaying() || !editSubjectSelected(EditorSelectionKind::PLAYER_START)) {
    return;
  }
  EditorPlayerStart& start =
      state_.document.player_starts[state_.selection.index];
  if (!player_start_prior_.has_value()) {
    player_start_prior_ = start;
  }
  start.character = character;
  commitPlayerStartEdit();
}

void SimplishEditor::applyClipEdit(const std::string& clip) {
  if (isPlaying() || !editSubjectSelected(EditorSelectionKind::PLACEMENT)) {
    return;
  }
  EditorPlacement& placement =
      state_.document.placements[state_.selection.index];
  if (!placement_prior_.has_value()) {
    placement_prior_ = placement;
  }
  placement.animation = clip;
  commitPlacementEdit();
}

void SimplishEditor::applyLightEdit(EditorPropertyField field, float value,
                                    EditorPropertyEdit edit) {
  EditorLight& light = state_.document.lights[state_.selection.index];
  if (!light_prior_.has_value()) {
    light_prior_ = light;
  }
  setEditorLightValue(light, field, value);
  if (edit == EditorPropertyEdit::COMMIT) {
    commitLightEdit();
  }
}

void SimplishEditor::applyPlayerStartEdit(EditorPropertyField field,
                                          float value,
                                          EditorPropertyEdit edit) {
  EditorPlayerStart& start =
      state_.document.player_starts[state_.selection.index];
  if (!player_start_prior_.has_value()) {
    player_start_prior_ = start;
  }
  setEditorPlayerStartValue(start, field, value);
  if (edit == EditorPropertyEdit::COMMIT) {
    commitPlayerStartEdit();
  }
}

void SimplishEditor::commitPendingEdit() {
  // One at most, never several: a gesture edits what is selected, and one
  // thing is selected. Each is offered the chance and the one holding a
  // prior takes it.
  commitPlacementEdit();
  commitLightEdit();
  commitPlayerStartEdit();
  commitWaypointEdit();
}

void SimplishEditor::applyWaypointEdit(EditorPropertyField field, float value,
                                       EditorPropertyEdit edit) {
  EditorWaypoint& waypoint = state_.document.waypoints[state_.selection.index];
  if (!waypoint_prior_.has_value()) {
    waypoint_prior_ = waypoint;
  }
  setEditorWaypointValue(waypoint, field, value);
  if (edit == EditorPropertyEdit::COMMIT) {
    commitWaypointEdit();
  }
}

void SimplishEditor::commitWaypointEdit() {
  if (!waypoint_prior_.has_value()) {
    return;
  }
  const auto prior = *std::exchange(waypoint_prior_, std::nullopt);
  if (!editSubjectSelected(EditorSelectionKind::WAYPOINT)) {
    return;
  }
  const auto& waypoint = state_.document.waypoints[state_.selection.index];
  if (sameWaypoint(prior, waypoint)) {
    return;
  }
  recordAction({.kind = EditorActionKind::TRANSFORM_WAYPOINT,
                .index = state_.selection.index,
                .waypoint = waypoint,
                .waypoint_prior = prior});
}

bool SimplishEditor::editSubjectSelected(EditorSelectionKind kind) const {
  return selectionIs(state_.selection, kind) &&
         state_.selection.index < selectionCount();
}

void SimplishEditor::recordAction(const EditorAction& action) {
  performEditorAction(state_.history, state_.document, action);
  applyEditToChrome();
}

void SimplishEditor::commitPlacementEdit() {
  if (!placement_prior_.has_value()) {
    return;
  }
  // Taken rather than read: the next gesture starts clean whether or not
  // this one turns out to be worth recording.
  const EditorPlacement prior = *std::exchange(placement_prior_, std::nullopt);
  if (!editSubjectSelected(EditorSelectionKind::PLACEMENT)) {
    return;
  }
  const EditorPlacement& placement =
      state_.document.placements[state_.selection.index];
  if (sameTransform(prior, placement)) {
    return;
  }
  // The placement already holds the new value, so the action is recorded
  // against it rather than applied over it.
  recordAction({.kind = EditorActionKind::TRANSFORM_PLACEMENT,
                .index = state_.selection.index,
                .placement = placement,
                .prior = prior});
}

void SimplishEditor::commitLightEdit() {
  if (!light_prior_.has_value()) {
    return;
  }
  const EditorLight prior = *std::exchange(light_prior_, std::nullopt);
  if (!editSubjectSelected(EditorSelectionKind::LIGHT)) {
    return;
  }
  const EditorLight& light = state_.document.lights[state_.selection.index];
  if (sameLight(prior, light)) {
    return;
  }
  recordAction({.kind = EditorActionKind::TRANSFORM_LIGHT,
                .index = state_.selection.index,
                .light = light,
                .light_prior = prior});
}

void SimplishEditor::commitPlayerStartEdit() {
  if (!player_start_prior_.has_value()) {
    return;
  }
  const auto prior = *std::exchange(player_start_prior_, std::nullopt);
  if (!editSubjectSelected(EditorSelectionKind::PLAYER_START)) {
    return;
  }
  const auto& start = state_.document.player_starts[state_.selection.index];
  if (sameStart(prior, start)) {
    return;
  }
  recordAction({.kind = EditorActionKind::TRANSFORM_PLAYER_START,
                .index = state_.selection.index,
                .player_start = start,
                .player_start_prior = prior});
}

void SimplishEditor::applyEditToChrome() {
  refreshUnsavedMarker();
  applySelectionToChrome();
  if (auto* menu = dynamic_cast<EditorMenuBarWidget*>(
          guiWidgetTree().findWidget(menu_bar_id_))) {
    menu->setHistory(state_.history);
    // Both are no-ops when nothing about them changed, which is every call
    // but the ones that follow a level being opened or created.
    menu->setLevels(state_.levels, state_.level_id);
  }
}

EditorPlacementMarker SimplishEditor::placementMarker(size_t index) {
  const EditorPlacement& placement = state_.document.placements[index];
  // An asset the list no longer has is measured as an empty one, which
  // reports the unit box on its tile rather than nothing at all.
  const EditorAsset& asset = placement.asset < state_.assets.size()
                                 ? state_.assets[placement.asset]
                                 : UNKNOWN_ASSET;
  return {placementWorldBounds(asset, placement),
          isSelected(EditorSelectionKind::PLACEMENT, index),
          placement.collides ? EditorMarkerStyle::FOOTPRINT
                             : EditorMarkerStyle::PASSABLE};
}

void SimplishEditor::appendPlacementMarkers(
    std::vector<EditorPlacementMarker>& markers) {
  size_t actor = 0;
  for (size_t i = 0; i < state_.document.placements.size(); ++i) {
    markers.push_back(isEditorActor(state_.document.placements[i])
                          ? actorMarker(i, actor++)
                          : placementMarker(i));
  }
}

EditorPlacementMarker SimplishEditor::actorMarker(size_t index, size_t actor) {
  const EditorPlacement placement = posedActor(index, actor);
  const EditorAsset& asset = placement.asset < state_.assets.size()
                                 ? state_.assets[placement.asset]
                                 : UNKNOWN_ASSET;
  EditorPlacementMarker marker{
      placementWorldBounds(asset, placement),
      isSelected(EditorSelectionKind::PLACEMENT, index),
      EditorMarkerStyle::ACTOR};
  marker.faction = placement.faction;
  marker.facing = editorActorFacing(placement);
  return marker;
}

EditorPlacementMarker SimplishEditor::waypointMarker(size_t index) {
  const EditorWaypoint& waypoint = state_.document.waypoints[index];
  EditorPlacementMarker marker{editorWaypointBounds(waypoint),
                               isSelected(EditorSelectionKind::WAYPOINT, index),
                               EditorMarkerStyle::WAYPOINT};
  marker.route = waypoint.route;
  return marker;
}

std::vector<EditorRouteLine> SimplishEditor::routeLines() const {
  std::vector<EditorRouteLine> lines;
  for (const uint8_t route : editorRoutesInUse(state_.document)) {
    const std::vector<Vec2> points = editorRoutePoints(state_.document, route);
    for (size_t i = 1; i < points.size(); ++i) {
      lines.push_back({{points[i - 1].x, points[i - 1].y, 0.0f},
                       {points[i].x, points[i].y, 0.0f},
                       route});
    }
  }
  return lines;
}

EditorPlacementMarker SimplishEditor::lightMarker(size_t index) {
  const EditorLight& light = state_.document.lights[index];
  // A light has no geometry, so its marker is a small box about where it
  // stands: something to see it by, and something to click.
  const Vec3 centre{light.position.x, light.position.y, light.position.z};
  const float reach = EDITOR_LIGHT_MARKER_RADIUS;
  return {{{centre.x - reach, centre.y - reach, centre.z - reach},
           {centre.x + reach, centre.y + reach, centre.z + reach}},
          isSelected(EditorSelectionKind::LIGHT, index)};
}

EditorPlacementMarker SimplishEditor::playerStartMarker(size_t index) {
  const EditorPlayerStart& start = state_.document.player_starts[index];
  return {editorPlayerStartBounds(start),
          isSelected(EditorSelectionKind::PLAYER_START, index),
          EditorMarkerStyle::PLAYER_START, start.player};
}

void SimplishEditor::refreshPlacementMarkers() {
  EditorViewportWidget* viewport = viewportWidget();
  if (viewport == nullptr) {
    return;
  }
  // Placements, then lights, then player starts, then waypoints: the
  // order `markerSelection` reads a pick back in.
  std::vector<EditorPlacementMarker>& markers = viewport->placement_markers;
  markers.clear();
  appendPlacementMarkers(markers);
  appendEntityMarkers(markers);
  appendPlaytestMarkers(markers);
  viewport->route_lines = routeLines();
  refreshOverlays();
}

void SimplishEditor::appendEntityMarkers(
    std::vector<EditorPlacementMarker>& markers) {
  for (size_t i = 0; i < state_.document.lights.size(); ++i) {
    markers.push_back(lightMarker(i));
  }
  for (size_t i = 0; i < state_.document.player_starts.size(); ++i) {
    markers.push_back(playerStartMarker(i));
  }
  for (size_t i = 0; i < state_.document.waypoints.size(); ++i) {
    markers.push_back(waypointMarker(i));
  }
}

void SimplishEditor::refreshOverlays() {
  // The level cannot change while it is played, so its grid is only
  // measured again while it is being edited.
  if (!isPlaying()) {
    refreshNavigationOverlay();
  }
  refreshActorOverlays();
}

void SimplishEditor::buildSceneLights() {
  scene_lights_.clear();
  for (const EditorLight& light : state_.document.lights) {
    // Past the shader's fixed loop the extra lights simply do not reach it;
    // dropping them here says so in one place rather than leaving the
    // renderer to truncate silently.
    if (scene_lights_.size() >= MESH_MAX_LIGHTS) {
      break;
    }
    scene_lights_.push_back(makeMeshLight(light));
  }
}

void SimplishEditor::appendSkinnedInstance(const EditorAsset& asset,
                                           const EditorPlacement& placement) {
  // The animator fades a prop whose clip was just changed in the panel,
  // rather than snapping it to the new clip's first frame.
  skinned_instances_.push_back(
      {asset.skinned_mesh, makePlacementTransform(asset, placement),
       asset.texture,
       placement_animator_.pose(placement, *asset.rig, animation_clock_)});
}

void SimplishEditor::appendPlacementInstance(const EditorPlacement& placement) {
  if (placement.asset >= state_.assets.size()) {
    return;
  }
  const EditorAsset& asset = state_.assets[placement.asset];
  if (asset.rig != nullptr && asset.skinned_mesh != MESH_GPU_INVALID) {
    appendSkinnedInstance(asset, placement);
  } else if (asset.mesh != MESH_GPU_INVALID) {
    scene_instances_.push_back(
        {asset.mesh, makePlacementTransform(asset, placement), asset.texture});
  }
}

void SimplishEditor::buildSceneInstances() {
  scene_instances_.clear();
  skinned_instances_.clear();
  size_t actor = 0;
  for (size_t i = 0; i < state_.document.placements.size(); ++i) {
    if (isEditorActor(state_.document.placements[i])) {
      appendActorInstance(i, actor++);
    } else {
      appendPlacementInstance(state_.document.placements[i]);
    }
  }
  appendCharacterInstances();
  // Players for props deleted since the last frame go; a prop brought back
  // by an undo gets a fresh one, which cuts in rather than fading. The
  // characters are posed first, so theirs are kept.
  placement_animator_.endFrame();
}

GuiColor SimplishEditor::frameClearColor() const {
  // What shows through the viewport, which paints no background of its own
  // so that the scene pass can draw there.
  return EDITOR_VIEWPORT_BG;
}

RhiTextureHandle SimplishEditor::sceneDepthTarget() {
  eng::RhiDevice* device = rhiDevice();
  // Nothing to draw means no scene pass at all, which leaves the frame
  // exactly as it was before any of this existed. A playtest always has
  // one: the players are drawn in it.
  if (device == nullptr || (state_.document.placements.empty() &&
                            !isPlaying() && characterFigures().empty())) {
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

float SimplishEditor::surfaceScale() {
  const auto width = static_cast<float>(guiLayoutWidth());
  return width > 0.0f ? static_cast<float>(backbufferWidth()) / width : 1.0f;
}

MeshStyle SimplishEditor::sceneStyle() const {
  return editorMeshStyleFor(state_.project.metadata.shading);
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
  params.lights = scene_lights_;
  params.viewport = surfaceViewport();
  params.scissor = toSurfaceScissor(viewport.rect, surfaceScale());
  params.shade_bands = sceneStyle().shade_bands;
  return params;
}

SkinnedMeshRenderer::DrawParams
SimplishEditor::skinnedDrawParams(const EditorViewportWidget& viewport) {
  const MeshRenderer::DrawParams scene = sceneDrawParams(viewport);
  SkinnedMeshRenderer::DrawParams params{};
  params.view_projection = scene.view_projection;
  params.instances = skinned_instances_;
  params.lights = scene.lights;
  params.viewport = scene.viewport;
  params.scissor = scene.scissor;
  params.shade_bands = scene.shade_bands;
  return params;
}

MeshOutlineRenderer::DrawParams
SimplishEditor::outlineDrawParams(const EditorViewportWidget& viewport) {
  const MeshRenderer::DrawParams scene = sceneDrawParams(viewport);
  const MeshStyle style = sceneStyle();
  MeshOutlineRenderer::DrawParams params{};
  params.depth = sceneDepthTarget();
  params.view_projection = scene.view_projection;
  // The style's width is in layout pixels, so a line is as thick on a
  // Retina display as on any other, just sharper.
  params.width = style.outline_width * surfaceScale();
  params.color = style.outline_color;
  params.viewport = scene.viewport;
  params.scissor = scene.scissor;
  return params;
}

void SimplishEditor::recordScene(RhiCommandList& cmd) {
  EditorViewportWidget* viewport = viewportWidget();
  if (viewport == nullptr) {
    return;
  }
  buildSceneInstances();
  buildSceneLights();
  mesh_renderer_.draw(cmd, sceneDrawParams(*viewport));
  // Same pass and depth as the static meshes, so a character walking
  // behind a crate is hidden by it, and the outline pass lines them both.
  skinned_renderer_.draw(cmd, skinnedDrawParams(*viewport));
}

void SimplishEditor::recordSceneOverlay(RhiCommandList& cmd) {
  EditorViewportWidget* viewport = viewportWidget();
  if (viewport == nullptr || sceneStyle().outline_width <= 0.0f) {
    return;
  }
  outline_renderer_.draw(cmd, outlineDrawParams(*viewport));
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
                     : isPlaying()                ? playtestStatus()
                                                  : formatStatus(*viewport));
  bar->setActiveTool(state_.active_tool);
  bar->setPlayMode(state_.playtest.mode);
  bar->tick(tree);
}

bool SimplishEditor::chromeNeedsLayout() {
  // Only when something the layout depends on actually changed; the chrome
  // is placed manually, so an unconditional pass would be wasted work. The
  // two panels are in here because folding one, or selecting a placement,
  // changes how much room the viewport beside it gets.
  return guiLayoutWidth() != laid_out_width_ ||
         guiLayoutHeight() != laid_out_height_ ||
         assetBrowserHeight() != laid_out_panel_height_ ||
         propertiesPanelWidth() != laid_out_properties_width_;
}

void SimplishEditor::setStateHook(std::function<bool(EditorShellState&)> hook) {
  state_hook_ = std::move(hook);
}

void SimplishEditor::runMenuCommand(EditorMenuCommand command) {
  executeCommand(command);
}

std::vector<std::string> SimplishEditor::assetIds() const {
  std::vector<std::string> ids;
  ids.reserve(state_.assets.size());
  for (const EditorAsset& asset : state_.assets) {
    ids.push_back(asset.id);
  }
  return ids;
}

void SimplishEditor::rebindPlacements(
    const std::vector<std::string>& previous_ids) {
  const size_t dropped =
      rebindPlacementAssets(state_.document, previous_ids, state_.assets);
  if (dropped > 0) {
    // Not an action, but the document no longer matches the file it came
    // from, and a save is what would make the two agree again.
    markEditorChangesUnsaved(state_.history);
  }
  reselectAfterRescan(dropped);
}

void SimplishEditor::reselectAfterRescan(size_t dropped) {
  // Nothing was dropped, so every index still names what it named and the
  // selection with it. Otherwise the list has shifted under it, and the
  // honest answer is to select nothing rather than something else.
  if (dropped == 0 ||
      !selectionIs(state_.selection, EditorSelectionKind::PLACEMENT)) {
    return;
  }
  LOG_INFO("editor", "Rescan dropped " + std::to_string(dropped) +
                         " placement(s) whose asset is gone");
  state_.selection = EditorSelection{};
}

void SimplishEditor::rescanAssets() {
  // The same project, so the document stays: a placement names its asset by
  // an id, and an id is what the new list can be searched for. Adding a
  // file to the project no longer costs the level everything in it.
  const std::vector<std::string> previous_ids = assetIds();
  reloadAssets();
  rebindPlacements(previous_ids);
  // The history describes the document by index into a list the rebind may
  // have shortened, so it cannot be replayed against what is there now.
  clearEditorActions(state_.history);
  placement_prior_.reset();
  light_prior_.reset();
  applyEditToChrome();
}

void SimplishEditor::syncViewState() {
  const EditorViewportWidget* viewport = viewportWidget();
  if (viewport == nullptr) {
    return;
  }
  state_.view = {viewport->camera,          viewport->hoveredTile(),
                 viewport->hasHover(),      viewport->show_grid,
                 viewport->show_navigation, viewport->show_ai};
}

void SimplishEditor::ensurePlacedMeshes() {
  for (const EditorPlacement& placement : state_.document.placements) {
    if (placement.asset < state_.assets.size()) {
      (void)ensureAssetMesh(placement.asset);
    }
  }
}

void SimplishEditor::runStateHook() {
  if (!state_hook_ || !state_hook_(state_)) {
    return;
  }
  // Only on a change: applyEditToChrome rewrites the properties panel from
  // the document, which would tear a property drag in progress out from
  // under the pointer if it ran on every tick.
  ensurePlacedMeshes();
  applyEditToChrome();
}

bool SimplishEditor::onTick(float dt) {
  syncViewState();
  runStateHook();
  tickPlaytest();
  if (chromeNeedsLayout()) {
    layoutChrome();
  }
  if (status_override_left_ > 0.0f) {
    status_override_left_ -= dt;
  }
  animation_clock_ += dt;
  tickMenuBar();
  refreshToolbar();
  pumpThumbnails();
  return !quit_requested_;
}

void SimplishEditor::tickMenuBar() {
  GuiWidgetTree& tree = guiWidgetTree();
  if (auto* menu =
          dynamic_cast<EditorMenuBarWidget*>(tree.findWidget(menu_bar_id_))) {
    menu->tick(tree);
  }
}

bool SimplishEditor::runDialogCommand(EditorMenuCommand command) {
  // Both answer later, on the main thread, through the on*Chosen overrides.
  if (command == EditorMenuCommand::NEW_PROJECT) {
    pending_dialog_ = EditorDialogPurpose::NEW_PROJECT;
    showSaveLocationDialog();
    return true;
  }
  if (command == EditorMenuCommand::NEW_LEVEL) {
    pending_dialog_ = EditorDialogPurpose::NEW_LEVEL;
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
  if (command == EditorMenuCommand::SAVE) {
    saveDocument();
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

void SimplishEditor::runUndo() {
  // A drag still in flight has already changed the document, so it is
  // recorded before the history is walked back — which makes it the thing
  // this undo reverts, rather than a change undo cannot reach.
  commitPendingEdit();
  if (isPlaying() || !canUndoEditorAction(state_.history)) {
    return;
  }
  // Read before the cursor moves: this is the action about to be undone,
  // and it is what says where the selection lands.
  const EditorAction action =
      state_.history.actions[state_.history.applied - 1];
  if (undoEditorAction(state_.history, state_.document)) {
    select(editorSelectionAfterUndo(action, state_.selection));
    applyEditToChrome();
  }
}

void SimplishEditor::runRedo() {
  commitPendingEdit();
  if (isPlaying() || !canRedoEditorAction(state_.history)) {
    return;
  }
  const EditorAction action = state_.history.actions[state_.history.applied];
  if (redoEditorAction(state_.history, state_.document)) {
    select(editorSelectionAfterRedo(action, state_.selection));
    applyEditToChrome();
  }
}

void SimplishEditor::runDelete() {
  // A drag still in flight has already moved the entry that is about to
  // go, so it is recorded first: undo then walks back the removal and the
  // move it interrupted, in that order, rather than losing the move.
  if (isPlaying()) {
    return;
  }
  commitPendingEdit();
  const std::optional<EditorAction> action =
      editorDeleteAction(state_.document, state_.selection);
  if (!action) {
    return;
  }
  // Moved off the entry before it stops existing: the panel is rebuilt
  // from what is selected, and the row this names is about to be gone.
  state_.selection = editorSelectionAfterRedo(*action, state_.selection);
  recordAction(*action);
}

bool SimplishEditor::runEditCommand(EditorMenuCommand command) {
  if (command == EditorMenuCommand::UNDO) {
    runUndo();
    return true;
  }
  if (command == EditorMenuCommand::REDO) {
    runRedo();
    return true;
  }
  if (command == EditorMenuCommand::DELETE_SELECTION) {
    runDelete();
    return true;
  }
  return false;
}

void SimplishEditor::executeCommand(EditorMenuCommand command) {
  if (runProjectCommand(command)) {
    return;
  }
  if (runEditCommand(command)) {
    return;
  }
  if (runPlaytestCommand(command)) {
    return;
  }
  if (command == EditorMenuCommand::ABOUT) {
    showAbout();
  } else {
    applyViewCommand(command);
  }
}

bool SimplishEditor::runPlaytestCommand(EditorMenuCommand command) {
  if (command == EditorMenuCommand::PLAYTEST) {
    togglePlaytest();
  } else if (command == EditorMenuCommand::PAUSE_PLAYTEST) {
    togglePlaytestPause();
  } else if (command == EditorMenuCommand::STEP_PLAYTEST) {
    stepPlaytest(1);
  } else if (const int stand_ins = editorStandInsOf(command); stand_ins >= 0) {
    setStandIns(static_cast<uint8_t>(stand_ins));
  } else {
    return false;
  }
  return true;
}

void SimplishEditor::setStandIns(uint8_t stand_ins) {
  state_.playtest_stand_ins = stand_ins;
  applyPlayModeToChrome();
  showStatusMessage(
      stand_ins == 0 ? "The next playtest is solo"
                     : "The next playtest adds " + std::to_string(stand_ins) +
                           " stand-in" + (stand_ins == 1 ? "" : "s"));
}

void SimplishEditor::applyViewCommand(EditorMenuCommand command) {
  // The projection and shading rows change the project; everything else on
  // the View menu only moves the camera over it.
  if (command == EditorMenuCommand::SET_VIEW_DIMETRIC) {
    applyProjection(ProjectProjection::DIMETRIC);
  } else if (command == EditorMenuCommand::SET_VIEW_ISOMETRIC) {
    applyProjection(ProjectProjection::ISOMETRIC);
  } else if (command == EditorMenuCommand::SET_SHADING_SMOOTH) {
    applyShading(ProjectShading::SMOOTH);
  } else if (command == EditorMenuCommand::SET_SHADING_CEL) {
    applyShading(ProjectShading::CEL);
  } else {
    applyCameraCommand(command);
  }
}

void SimplishEditor::applyCameraCommand(EditorMenuCommand command) {
  auto* viewport = dynamic_cast<EditorViewportWidget*>(
      guiWidgetTree().findWidget(viewport_id_));
  if (viewport == nullptr) {
    return;
  }
  if (command == EditorMenuCommand::RESET_VIEW) {
    // Everything but the projection: that is the project's setting, not
    // part of where the camera happens to be looking right now.
    viewport->camera = IsoCamera{.axes = viewport->camera.axes};
  } else if (command == EditorMenuCommand::ZOOM_IN) {
    zoomAtCentre(*viewport, 1.0f);
  } else if (command == EditorMenuCommand::ZOOM_OUT) {
    zoomAtCentre(*viewport, -1.0f);
  } else if (command == EditorMenuCommand::TOGGLE_GRID) {
    viewport->show_grid = !viewport->show_grid;
  } else {
    toggleOverlay(*viewport, command);
  }
}

void SimplishEditor::toggleOverlay(EditorViewportWidget& viewport,
                                   EditorMenuCommand command) {
  if (command == EditorMenuCommand::TOGGLE_NAVIGATION) {
    viewport.show_navigation = !viewport.show_navigation;
    refreshNavigationOverlay();
    reportNavigation();
  } else if (command == EditorMenuCommand::TOGGLE_AI_OVERLAY) {
    viewport.show_ai = !viewport.show_ai;
    refreshActorOverlays();
  }
}

void SimplishEditor::refreshNavigationOverlay() {
  EditorViewportWidget* viewport = viewportWidget();
  if (viewport == nullptr) {
    return;
  }
  viewport->nav_overlay = viewport->show_navigation
                              ? editorNavOverlay(analyseEditorNavigation(
                                    state_.document, state_.assets))
                              : EditorNavOverlay{};
}

void SimplishEditor::reportNavigation() {
  const EditorViewportWidget* viewport = viewportWidget();
  if (viewport == nullptr || !viewport->show_navigation) {
    return;
  }
  const EditorNavigation navigation =
      analyseEditorNavigation(state_.document, state_.assets);
  const size_t stuck =
      navigation.unreachable_actors.size() + navigation.stranded_actors.size();
  showStatusMessage(stuck == 0
                        ? "Navigation: every actor can reach a player start"
                        : "Navigation: " + std::to_string(stuck) +
                              " actor(s) cannot reach a player start — see "
                              "get_navigation");
}

void SimplishEditor::applyProjection(ProjectProjection projection) {
  if (!state_.project.loaded ||
      state_.project.metadata.projection == projection) {
    return;
  }
  state_.project.metadata.projection = projection;
  applyProjectionToWidgets();
  // Every card is drawn at the viewport's own angle, so they are all now
  // pictures of the wrong one. They rebuild as their cards scroll into view.
  releaseAssetThumbnails();
  if (!saveProjectMetadata(state_.project)) {
    // The editor is already showing the new projection, so this is a
    // warning rather than a refusal: what is lost is only its persistence.
    LOG_WARN("editor", "Could not write the projection to project.json");
    showStatusMessage("Switched view, but could not save it to the project");
  }
}

void SimplishEditor::applyProjectionToWidgets() {
  const IsoAxes axes = isoAxesFor(state_.project.metadata.projection);
  if (auto* viewport = viewportWidget()) {
    viewport->camera.axes = axes;
  }
  state_.view.camera.axes = axes;
  if (auto* menu = dynamic_cast<EditorMenuBarWidget*>(
          guiWidgetTree().findWidget(menu_bar_id_))) {
    menu->setProjection(state_.project.metadata.projection);
  }
}

void SimplishEditor::applyShading(ProjectShading shading) {
  if (!state_.project.loaded || state_.project.metadata.shading == shading) {
    return;
  }
  // Nothing to rebuild: the scene pass reads the setting every frame, so
  // the next one is drawn in the new style. Thumbnails stay as they are —
  // a card is for recognising an asset, and it looks the same either way.
  state_.project.metadata.shading = shading;
  applyShadingToWidgets();
  if (!saveProjectMetadata(state_.project)) {
    LOG_WARN("editor", "Could not write the shading to project.json");
    showStatusMessage("Switched shading, but could not save it to the project");
  }
}

void SimplishEditor::applyShadingToWidgets() {
  if (auto* menu = dynamic_cast<EditorMenuBarWidget*>(
          guiWidgetTree().findWidget(menu_bar_id_))) {
    menu->setShading(state_.project.metadata.shading);
  }
}

void SimplishEditor::onSaveLocationChosen(const std::filesystem::path& path) {
  if (pending_dialog_ == EditorDialogPurpose::NEW_LEVEL) {
    // The name typed, not the folder it was typed into: a level file lives
    // under the project's own content/levels wherever the dialog was
    // pointed, and the id is what the format cares about.
    createLevel(editorLevelIdFromText(path.stem().string()),
                EditorLevelUnsaved::REFUSE);
    return;
  }
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

bool SimplishEditor::handleEditKey(uint32_t key, ClientKeyModifiers modifiers) {
  // Control and Command are both accepted everywhere, which is what the
  // GUI's own clipboard keys already do — and what someone arriving from
  // either convention will reach for.
  if (!modifiers.ctrl && !modifiers.gui) {
    return false;
  }
  if (key != 'z' && key != 'Z') {
    return false;
  }
  executeCommand(modifiers.shift ? EditorMenuCommand::REDO
                                 : EditorMenuCommand::UNDO);
  return true;
}

bool SimplishEditor::handleFileKey(uint32_t key, ClientKeyModifiers modifiers) {
  // Control and Command both, as the Edit accelerators already accept both.
  if ((!modifiers.ctrl && !modifiers.gui) || (key != 's' && key != 'S')) {
    return false;
  }
  executeCommand(EditorMenuCommand::SAVE);
  return true;
}

bool SimplishEditor::handleSelectionKey(uint32_t key) {
  // Escape drops the selection, which is also what puts the properties
  // panel away and gives the viewport its width back.
  if (key == eng::client::DesktopPlatformKeycode::ESCAPE) {
    select({});
    return true;
  }
  // Backspace and Delete are one gesture on a keyboard that has both, and
  // on the Mac layouts that label Backspace "delete" they are the only one
  // — so both remove what is selected rather than one of them doing
  // nothing on half the machines this runs on.
  if (key != eng::client::DesktopPlatformKeycode::BACKSPACE &&
      key != eng::client::DesktopPlatformKeycode::DELETE_FORWARD) {
    return false;
  }
  runDelete();
  return true;
}

void SimplishEditor::onClientKeyDown(uint32_t key, ClientKeyDownKind kind,
                                     ClientKeyModifiers modifiers) {
  if (handlePlaytestKey(key, kind)) {
    return;
  }
  if (isPlaying()) {
    // Only the view keys stay live while playing: zooming out to see more
    // of the level is fair, and nothing may edit it.
    if (kind == ClientKeyDownKind::FIRST_PRESS) {
      (void)handleViewKey(key);
    }
    return;
  }
  handleEditingKey(key, kind, modifiers);
}

void SimplishEditor::handleEditingKey(uint32_t key, ClientKeyDownKind kind,
                                      ClientKeyModifiers modifiers) {
  // Before the repeat guard: holding the accelerator to walk back through a
  // run of edits is most of what the gesture is for, and undo stops on its
  // own once the history runs out.
  if (handleEditKey(key, modifiers)) {
    return;
  }
  if (kind == ClientKeyDownKind::REPEAT) {
    return;
  }
  // First-press only, unlike undo: holding the key would write the same
  // file over and over for as long as it was down.
  if (handleFileKey(key, modifiers)) {
    return;
  }
  if (handleSelectionKey(key)) {
    return;
  }
  if (handleViewKey(key)) {
    return;
  }
  handleToolKey(key);
}

void SimplishEditor::handleToolKey(uint32_t key) {
  // Number keys select tools, matching the toolbar's left-to-right order.
  constexpr uint32_t KEY_1 = '1';
  const auto count = static_cast<uint32_t>(std::size(EDITOR_TOOLS));
  if (key >= KEY_1 && key < KEY_1 + count) {
    state_.active_tool = EDITOR_TOOLS[key - KEY_1];
  }
}

void SimplishEditor::onShutdown() {
  releaseAssetThumbnails();
  releaseAssetTextures();
  shutdownChrome();
  if (rhiDevice() != nullptr) {
    outline_renderer_.shutdown(*rhiDevice());
    skinned_renderer_.shutdown(*rhiDevice());
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
  for (GuiWidgetId* id : {&character_select_id_, &asset_panel_id_,
                          &properties_panel_id_, &menu_bar_id_, &toolbar_id_,
                          &viewport_id_, &title_panel_, &root_panel_}) {
    tree.destroyWidget(*id);
    *id = GUI_WIDGET_ID_INVALID;
  }
  // Went with the title panel it sits in.
  title_label_ = GUI_WIDGET_ID_INVALID;
}

}  // namespace eng::editor
