#include <algorithm>
#include <cmath>
#include <editor/project/project-ops.h>
#include <editor/project/project-paths.h>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-asset-scan.h>
#include <editor/shell/editor-asset-thumbnail.h>
#include <editor/shell/editor-asset-tree.h>
#include <editor/shell/editor-general-section.h>
#include <editor/shell/editor-light-ops.h>
#include <editor/shell/editor-placement-transform.h>
#include <editor/shell/editor-property-ops.h>
#include <editor/shell/editor-thumbnail-cache.h>
#include <editor/shell/iso-view-matrix.h>
#include <editor/shell/simplish-editor.h>
#include <engine/client/desktop-platform-keycode.h>
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

  /// Whether two placements sit and face exactly the same way.
  ///
  /// A gesture that ended where it began is not an edit, and an undo entry
  /// that changes nothing is worse than no entry at all. Exact comparison
  /// is right here: the values come from the same arithmetic on both sides,
  /// and a tolerance would swallow the smallest nudge the panel can make.
  bool sameTransform(const EditorPlacement& a, const EditorPlacement& b) {
    return a.position.x == b.position.x && a.position.y == b.position.y &&
           a.position.z == b.position.z && a.rotation.x == b.rotation.x &&
           a.rotation.y == b.rotation.y && a.rotation.z == b.rotation.z;
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

  /// Stands in for an asset a placement names but the list no longer has.
  ///
  /// A reference to this rather than a copy of the real one: markers are
  /// rebuilt on every frame of a property drag, and an EditorAsset carries
  /// two paths and a name.
  const EditorAsset UNKNOWN_ASSET{};

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
  properties_panel_id_ =
      tree.insertExternalWidget(std::move(panel), root_panel_);
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
  auto viewport = std::make_unique<EditorViewportWidget>();
  viewport->on_placement_picked = [this](int marker) {
    selectMarker(marker);
  };
  viewport_id_ = tree.insertExternalWidget(std::move(viewport), root_panel_);
  initPropertiesPanel(tree);
  initAssetPanel(tree);
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
  // they go with it — and the history with them, since every action names an
  // entry by an index that is about to mean something else. The lights go
  // too: they name no asset, but they belong to the level being closed, and
  // leaving them behind would light the next project with them. Nothing is
  // persisted yet either way.
  state_.document = EditorDocument{};
  state_.selection = EditorSelection{};
  placement_prior_.reset();
  light_prior_.reset();
  clearEditorActions(state_.history);
  // The textures belong to the list about to be replaced, and nothing else
  // will ever hold their handles again.
  releaseAssetThumbnails();
  EditorAssetScan scan =
      state_.project.loaded
          ? scanEditorAssets(projectAssetsPath(state_.project.root))
          : EditorAssetScan{};
  state_.asset_tree = buildEditorAssetTree(scan);
  state_.assets = std::move(scan.assets);
  // The built-in items are numbered after the scanned assets, which is the
  // numbering the browser reports a drop in and the tree holds.
  appendEditorGeneralSection(state_.asset_tree, state_.assets.size());
  refreshAssetPanel();
  applyEditToChrome();
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

ImageData SimplishEditor::buildAssetThumbnail(const EditorAsset& asset) {
  const ThumbnailCacheEntry entry{projectThumbnailsPath(state_.project.root),
                                  asset.path, asset.relative_path};
  if (std::optional<ImageData> cached = loadCachedThumbnail(entry)) {
    return std::move(*cached);
  }
  // The expensive half, and the reason the cache exists: parsing a mesh the
  // editor may never otherwise need to read.
  std::optional<MeshData> mesh = loadObjMesh(asset.path);
  if (!mesh.has_value()) {
    return {};
  }
  orientYUpToZUp(*mesh);
  ImageData image = renderAssetThumbnail(*mesh, ASSET_THUMBNAIL_SIZE);
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

void SimplishEditor::dropBrowserEntry(size_t entry, float x, float y) {
  EditorViewportWidget* viewport = viewportWidget();
  // A drop anywhere but the viewport is not a placement.
  if (viewport == nullptr || !containsPoint(viewport->rect, x, y)) {
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
    placeLight(EDITOR_GENERAL_ITEMS[item], tile);
  }
}

void SimplishEditor::placeAsset(size_t index, WorldPoint position) {
  // Appended, so undo takes the newest placement off the end and redo puts
  // it back at the same index.
  const size_t placed = state_.document.placements.size();
  recordAction({.kind = EditorActionKind::PLACE_ASSET,
                .index = placed,
                .placement = {index, position, {}}});
  // Selecting what was just dropped opens the panel on it, which is what
  // somebody who wants it a quarter-tile to the left is about to reach for.
  select({EditorSelectionKind::PLACEMENT, placed});
}

void SimplishEditor::placeLight(EditorGeneralItem item, WorldPoint tile) {
  // Over the middle of the tile it was dropped on, and above head height,
  // so a point light lights what is around it rather than sitting inside a
  // prop standing there.
  const WorldPoint position{tile.x + 0.5f, tile.y + 0.5f,
                            EDITOR_LIGHT_DROP_HEIGHT};
  const size_t added = state_.document.lights.size();
  recordAction(
      {.kind = EditorActionKind::ADD_LIGHT,
       .index = added,
       .light = makeEditorLight(editorGeneralItemLightKind(item), position)});
  select({EditorSelectionKind::LIGHT, added});
}

size_t SimplishEditor::selectionCount() const {
  switch (state_.selection.kind) {
    case EditorSelectionKind::PLACEMENT:
      return state_.document.placements.size();
    case EditorSelectionKind::LIGHT:
      return state_.document.lights.size();
    case EditorSelectionKind::NONE:
      // Nothing selected has no list, so no index is ever in range — which
      // is what every caller here asks this in order to find out.
      return 0;
  }
  return 0;
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
  if (marker < 0) {
    select({});
    return;
  }
  // Markers are the placements and then the lights, so which list a marker
  // names is which half of that run it falls in.
  const auto index = static_cast<size_t>(marker);
  const size_t placements = state_.document.placements.size();
  select(index < placements
             ? EditorSelection{EditorSelectionKind::PLACEMENT, index}
             : EditorSelection{EditorSelectionKind::LIGHT, index - placements});
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
}

void SimplishEditor::showLightSelection(EditorPropertiesWidget& panel) {
  const EditorLight& light = state_.document.lights[state_.selection.index];
  panel.setSelection(std::string(editorLightKindName(light.kind)), light);
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
  } else {
    showLightSelection(*panel);
  }
  refreshPlacementMarkers();
}

void SimplishEditor::applyPropertyEdit(EditorPropertyField field, float value,
                                       EditorPropertyEdit edit) {
  if (state_.selection.index >= selectionCount()) {
    return;
  }
  if (selectionIs(state_.selection, EditorSelectionKind::PLACEMENT)) {
    applyPlacementEdit(field, value, edit);
  } else {
    applyLightEdit(field, value, edit);
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

void SimplishEditor::commitPendingEdit() {
  // One or the other, never both: a gesture edits what is selected, and one
  // thing is selected. Each is offered the chance and the one holding a
  // prior takes it.
  commitPlacementEdit();
  commitLightEdit();
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

void SimplishEditor::applyEditToChrome() {
  applySelectionToChrome();
  if (auto* menu = dynamic_cast<EditorMenuBarWidget*>(
          guiWidgetTree().findWidget(menu_bar_id_))) {
    menu->setHistory(state_.history);
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
          isSelected(EditorSelectionKind::PLACEMENT, index)};
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

void SimplishEditor::refreshPlacementMarkers() {
  EditorViewportWidget* viewport = viewportWidget();
  if (viewport == nullptr) {
    return;
  }
  const EditorDocument& document = state_.document;
  viewport->placement_markers.clear();
  viewport->placement_markers.reserve(document.placements.size() +
                                      document.lights.size());
  // Placements first and lights after, which is the order `selectMarker`
  // reads a pick back in.
  for (size_t i = 0; i < document.placements.size(); ++i) {
    viewport->placement_markers.push_back(placementMarker(i));
  }
  for (size_t i = 0; i < document.lights.size(); ++i) {
    viewport->placement_markers.push_back(lightMarker(i));
  }
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

void SimplishEditor::buildSceneInstances() {
  scene_instances_.clear();
  for (const auto& placement : state_.document.placements) {
    if (placement.asset >= state_.assets.size()) {
      continue;
    }
    const EditorAsset& asset = state_.assets[placement.asset];
    if (asset.mesh == MESH_GPU_INVALID) {
      continue;
    }
    scene_instances_.push_back(
        {asset.mesh, makePlacementTransform(asset, placement)});
  }
}

GuiColor SimplishEditor::frameClearColor() const {
  // What shows through the viewport, which paints no background of its own
  // so that the scene pass can draw there.
  return EDITOR_VIEWPORT_BG;
}

RhiTextureHandle SimplishEditor::sceneDepthTarget() {
  eng::RhiDevice* device = rhiDevice();
  // No placements means no scene pass at all, which leaves the frame
  // exactly as it was before any of this existed.
  if (device == nullptr || state_.document.placements.empty()) {
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
  params.lights = scene_lights_;
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
  buildSceneLights();
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

bool SimplishEditor::onTick(float dt) {
  if (chromeNeedsLayout()) {
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
  pumpThumbnails();
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

void SimplishEditor::runUndo() {
  // A drag still in flight has already changed the document, so it is
  // recorded before the history is walked back — which makes it the thing
  // this undo reverts, rather than a change undo cannot reach.
  commitPendingEdit();
  if (!canUndoEditorAction(state_.history)) {
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
  if (!canRedoEditorAction(state_.history)) {
    return;
  }
  const EditorAction action = state_.history.actions[state_.history.applied];
  if (redoEditorAction(state_.history, state_.document)) {
    select(editorSelectionAfterRedo(action, state_.selection));
    applyEditToChrome();
  }
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
  return false;
}

void SimplishEditor::executeCommand(EditorMenuCommand command) {
  if (runProjectCommand(command)) {
    return;
  }
  if (runEditCommand(command)) {
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

bool SimplishEditor::handleSelectionKey(uint32_t key) {
  // Escape drops the selection, which is also what puts the properties
  // panel away and gives the viewport its width back.
  if (key != eng::client::DesktopPlatformKeycode::ESCAPE) {
    return false;
  }
  select({});
  return true;
}

void SimplishEditor::onClientKeyDown(uint32_t key, ClientKeyDownKind kind,
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
  tree.destroyWidget(properties_panel_id_);
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
  properties_panel_id_ = GUI_WIDGET_ID_INVALID;
}

}  // namespace eng::editor
