#include <cmath>
#include <cstdint>
#include <editor/shell/editor-viewport-widget.h>
#include <engine/gui/gui-color.h>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-renderer.h>
#include <engine/gui/gui-theme-constants.h>

namespace eng::editor {

namespace {

  constexpr GuiColor VIEWPORT_BG{22, 22, 26, 255};
  constexpr GuiColor GRID_LINE{44, 44, 50, 255};
  constexpr GuiColor GRID_MAJOR{64, 64, 72, 255};
  constexpr GuiColor AXIS_X{200, 70, 70, 255};
  constexpr GuiColor AXIS_Y{70, 180, 90, 255};
  constexpr GuiColor HOVER_FILL{0, 122, 204, 90};

  /// Tiles drawn either side of the focus point. Bounded rather than derived
  /// from the viewport so a zoomed-out view cannot emit an unbounded number
  /// of lines.
  constexpr int GRID_EXTENT = 64;
  /// Every Nth grid line is drawn in the brighter major colour.
  constexpr int GRID_MAJOR_EVERY = 8;

  constexpr float LABEL_INSET = 8.0f;

  void emitIsoLine(GuiRendererContext& renderer, IsoPoint a, IsoPoint b,
                   uint32_t color) {
    renderer.emitLine({a.x, a.y, b.x, b.y, color});
  }

  /// Draw the two families of tile-edge lines: constant world X and
  /// constant world Y. The camera has zero yaw, so in screen space these are
  /// axis-aligned — verticals and foreshortened horizontals.
  void renderGrid(GuiRendererContext& renderer, const IsoView& view) {
    const uint32_t minor = GRID_LINE.pack();
    const uint32_t major = GRID_MAJOR.pack();
    const auto extent = static_cast<float>(GRID_EXTENT);

    for (int i = -GRID_EXTENT; i <= GRID_EXTENT; ++i) {
      const auto line = static_cast<float>(i);
      const uint32_t color = (i % GRID_MAJOR_EVERY == 0) ? major : minor;

      emitIsoLine(renderer, worldToScreen(view, {line, -extent}),
                  worldToScreen(view, {line, extent}), color);
      emitIsoLine(renderer, worldToScreen(view, {-extent, line}),
                  worldToScreen(view, {extent, line}), color);
    }
  }

  /// Draw the +X and +Y world axes from the origin so orientation is
  /// unambiguous at any pan position.
  void renderAxes(GuiRendererContext& renderer, const IsoView& view) {
    constexpr float AXIS_LEN = 8.0f;
    const IsoPoint origin = worldToScreen(view, {0.0f, 0.0f});
    emitIsoLine(renderer, origin, worldToScreen(view, {AXIS_LEN, 0.0f}),
                AXIS_X.pack());
    emitIsoLine(renderer, origin, worldToScreen(view, {0.0f, AXIS_LEN}),
                AXIS_Y.pack());
  }

  /// Outline the rectangular footprint of a single tile.
  void renderTileHighlight(GuiRendererContext& renderer, const IsoView& view,
                           WorldPoint tile) {
    const uint32_t color = HOVER_FILL.pack();
    const IsoPoint corners[] = {
        worldToScreen(view, {tile.x, tile.y}),
        worldToScreen(view, {tile.x + 1.0f, tile.y}),
        worldToScreen(view, {tile.x + 1.0f, tile.y + 1.0f}),
        worldToScreen(view, {tile.x, tile.y + 1.0f}),
    };
    for (size_t i = 0; i < 4; ++i) {
      emitIsoLine(renderer, corners[i], corners[(i + 1) % 4], color);
    }
  }

}  // namespace

EditorViewportWidget::EditorViewportWidget() {
  widget_type = GuiWidgetType::CUSTOM;
  debug_name = "editor-viewport";
}

std::unique_ptr<GuiWidget> EditorViewportWidget::clone() const {
  return std::make_unique<EditorViewportWidget>(*this);
}

void EditorViewportWidget::render(const GuiDrawContext& ctx) const {
  if (rect.w <= 0.0f || rect.h <= 0.0f) {
    return;
  }

  const auto bg = GuiColor::applyOpacity(VIEWPORT_BG, opacity);
  ctx.drawFilledRect(rect, bg);

  if (ctx.renderer != nullptr) {
    const IsoView view = makeIsoView(camera, rect);
    // Grid lines run past the viewport by design; scissor keeps them inside.
    ctx.renderer->pushScissor(rect);
    renderGrid(*ctx.renderer, view);
    renderAxes(*ctx.renderer, view);
    if (has_hover_) {
      renderTileHighlight(*ctx.renderer, view, hovered_tile_);
    }
    ctx.renderer->popScissor();
  }

  ctx.drawBorderRect(rect, GuiColor::applyOpacity(THEME_BORDER, opacity));
  ctx.drawText(GuiColor::applyOpacity(THEME_DIM, opacity),
               drawPosInset(rect, LABEL_INSET, LABEL_INSET), "Level");
}

bool EditorViewportWidget::handleMouseDown(const GuiMouseEvent& event) {
  const bool pan_button =
      event.button == GuiMouseButton::MIDDLE ||
      (event.button == GuiMouseButton::LEFT && event.shift_held);
  if (!pan_button) {
    return false;
  }
  panning_ = true;
  drag_last_x_ = event.x;
  drag_last_y_ = event.y;
  return true;
}

void EditorViewportWidget::handleMouseUp(const GuiMouseEvent& /*event*/) {
  panning_ = false;
}

void EditorViewportWidget::handleMouseMove(const GuiMouseEvent& event) {
  if (panning_) {
    panCamera(camera, event.x - drag_last_x_, event.y - drag_last_y_);
    drag_last_x_ = event.x;
    drag_last_y_ = event.y;
    return;
  }
  updateHover(event.x, event.y);
}

bool EditorViewportWidget::handleScroll(const GuiScrollEvent& event) {
  zoomCameraAt(camera, rect, event.delta_y, event.x, event.y);
  updateHover(event.x, event.y);
  return true;
}

void EditorViewportWidget::updateHover(float x, float y) {
  if (!containsPoint(rect, x, y)) {
    has_hover_ = false;
    return;
  }
  const IsoView view = makeIsoView(camera, rect);
  const WorldPoint world = screenToWorld(view, {x, y});
  hovered_tile_ = {std::floor(world.x), std::floor(world.y)};
  has_hover_ = true;
}

}  // namespace eng::editor
