#include "engine/gui/gui-viewport.h"

#include "engine/gui/gui-color.h"
#include "engine/gui/gui-draw-context.h"
#include "engine/gui/gui-rect.h"
#include "engine/gui/gui-renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace eng {

std::unique_ptr<GuiWidget> GuiViewport::clone() const {
  return std::make_unique<GuiViewport>(*this);
}

namespace {

  // ─── Camera constants ──────────────────────────────────────
  constexpr float ORBIT_SENSITIVITY = 0.008f;
  constexpr float PAN_SENSITIVITY = 1.0f;
  constexpr float ZOOM_SENSITIVITY = 20.0f;
  constexpr float MIN_DISTANCE = 50.0f;
  constexpr float MAX_DISTANCE = 2000.0f;
  constexpr float MIN_PITCH = -1.5f;
  constexpr float MAX_PITCH = 1.5f;

  // ─── Grid constants ───────────────────────────────────────
  constexpr int GRID_EXTENT = 10;
  constexpr int GRID_SPACING = 40;

  // ─── Colors ───────────────────────────────────────────────
  constexpr GuiColor VIEWPORT_COLOR{24, 24, 28, 255};
  constexpr GuiColor GRID_LINE_COLOR{50, 50, 54, 255};
  constexpr GuiColor AXIS_X_COLOR{220, 60, 60, 255};
  constexpr GuiColor AXIS_Y_COLOR{60, 220, 60, 255};
  constexpr GuiColor AXIS_Z_COLOR{60, 60, 220, 255};
  constexpr GuiColor LABEL_COLOR{200, 200, 200, 255};

  // ─── Label layout ──────────────────────────────────────────
  /// Horizontal inset for the viewport title label.
  constexpr int LABEL_INSET_X = 8;
  /// Vertical inset for the viewport title label.
  constexpr int LABEL_INSET_Y = 6;

  // ─── Projection ───────────────────────────────────────────

  /// Pre-computed camera transform for world-to-screen projection.
  struct CameraProjection {
    /// Viewport center X.
    float vp_cx = 0.0f;
    /// Viewport center Y.
    float vp_cy = 0.0f;
    /// Cosine of camera yaw.
    float cos_yaw = 0.0f;
    /// Sine of camera yaw.
    float sin_yaw = 0.0f;
    /// Cosine of camera pitch.
    float cos_pitch = 0.0f;
    /// Sine of camera pitch.
    float sin_pitch = 0.0f;
    /// Camera target X.
    float target_x = 0.0f;
    /// Camera target Y.
    float target_y = 0.0f;
    /// Camera distance from target.
    float distance = 0.0f;
  };

  /// 3D world-space point for projection.
  struct WorldPoint {
    /// X coordinate.
    float x = 0;
    /// Y coordinate.
    float y = 0;
    /// Z coordinate.
    float z = 0;
  };

  /// 2D screen-space point.
  struct ScreenPoint {
    /// X coordinate.
    float x = 0;
    /// Y coordinate.
    float y = 0;
  };

  /// Minimum depth to prevent division by zero in projection.
  constexpr float MIN_PROJ_DEPTH = 10.0f;
  /// Multiplier for computing centered offsets.
  constexpr float HALF = 0.5f;

  CameraProjection buildProjection(const ViewportCamera& cam, const Rect& vp) {
    CameraProjection proj;
    proj.vp_cx = vp.x + vp.w * HALF;
    proj.vp_cy = vp.y + vp.h * HALF;
    proj.cos_yaw = std::cos(cam.yaw);
    proj.sin_yaw = std::sin(cam.yaw);
    proj.cos_pitch = std::cos(cam.pitch);
    proj.sin_pitch = std::sin(cam.pitch);
    proj.target_x = cam.target_x;
    proj.target_y = cam.target_y;
    proj.distance = cam.distance;
    return proj;
  }

  ScreenPoint projectPoint(const CameraProjection& proj, WorldPoint wp) {
    float rx = wp.x - proj.target_x;
    float ry = wp.y - proj.target_y;
    float tx = rx * proj.cos_yaw - ry * proj.sin_yaw;
    float ty = rx * proj.sin_yaw + ry * proj.cos_yaw;
    float depth = std::max(proj.distance + ty * proj.cos_pitch, MIN_PROJ_DEPTH);
    float scale = proj.distance / depth;
    return {proj.vp_cx + tx * scale,
            proj.vp_cy - wp.z * scale + ty * proj.sin_pitch * scale};
  }

  // ─── Grid rendering ──────────────────────────────────────

  /// Parameters for rendering a single grid line.
  struct GridLineParams {
    /// Renderer context.
    GuiRendererContext& renderer;
    /// Camera projection.
    const CameraProjection& proj;
    /// Grid index position.
    float gi;
    /// Grid extent.
    float ext;
  };

  void renderGridLine(const GridLineParams& p, uint32_t color) {
    auto& renderer = p.renderer;
    const auto& proj = p.proj;
    float gi = p.gi;
    float ext = p.ext;
    auto a0 = projectPoint(proj, {gi, -ext, 0});
    auto a1 = projectPoint(proj, {gi, ext, 0});
    renderer.emitLine({a0.x, a0.y, a1.x, a1.y, color});
    auto b0 = projectPoint(proj, {-ext, gi, 0});
    auto b1 = projectPoint(proj, {ext, gi, 0});
    renderer.emitLine({b0.x, b0.y, b1.x, b1.y, color});
  }

  void renderGrid(GuiRendererContext& renderer, const CameraProjection& proj) {
    uint32_t color = GRID_LINE_COLOR.pack();
    auto ext = static_cast<float>(GRID_EXTENT * GRID_SPACING);
    for (int i = -GRID_EXTENT; i <= GRID_EXTENT; ++i) {
      renderGridLine(
          {renderer, proj, static_cast<float>(i * GRID_SPACING), ext}, color);
    }
  }

  // ─── Axis rendering ──────────────────────────────────────

  /// Length of drawn axis lines in world units.
  constexpr float AXIS_LEN = 80.0f;

  /// A colored axis line to draw from origin.
  struct AxisDef {
    /// World-space endpoint of the axis.
    WorldPoint tip;
    /// Color for the axis line.
    GuiColor color;
  };

  void renderAxes(GuiRendererContext& renderer, const CameraProjection& proj) {
    auto origin = projectPoint(proj, {0, 0, 0});
    constexpr AxisDef AXES[] = {
        {{AXIS_LEN, 0, 0}, AXIS_X_COLOR},
        {{0, AXIS_LEN, 0}, AXIS_Y_COLOR},
        {{0, 0, AXIS_LEN}, AXIS_Z_COLOR},
    };
    for (const auto& axis : AXES) {
      auto tip = projectPoint(proj, axis.tip);
      renderer.emitLine({origin.x, origin.y, tip.x, tip.y, axis.color.pack()});
    }
  }

}  // namespace

void GuiViewport::render(const GuiDrawContext& ctx) const {
  auto vc = GuiColor::applyOpacity(VIEWPORT_COLOR, opacity);
  ctx.drawFilledRect(rect, vc);
  ctx.drawBorderRect(rect, vc);
  if (ctx.renderer != nullptr) {
    auto proj = buildProjection(camera, rect);
    renderGrid(*ctx.renderer, proj);
    renderAxes(*ctx.renderer, proj);
  }
  auto lc = GuiColor::applyOpacity(LABEL_COLOR, opacity);
  ctx.drawText(lc, drawPosInset(rect, LABEL_INSET_X, LABEL_INSET_Y),
               "3D Viewport");
}

bool GuiViewport::handleMouseDown(const GuiMouseEvent& event) {
  bool is_pan = event.button == GuiMouseButton::MIDDLE;
  if (event.button == GuiMouseButton::LEFT && event.shift_held) {
    is_pan = true;
  }
  dragging_ = true;
  panning_ = is_pan;
  drag_last_x_ = event.x;
  drag_last_y_ = event.y;
  GuiWidget::handleMouseDown(event);
  return true;
}

void GuiViewport::handleMouseUp(const GuiMouseEvent& event) {
  dragging_ = false;
  panning_ = false;
  GuiWidget::handleMouseUp(event);
}

void GuiViewport::applyPan(float dx, float dy) {
  float cos_yaw = std::cos(camera.yaw);
  float sin_yaw = std::sin(camera.yaw);
  camera.target_x -= (dx * cos_yaw - dy * sin_yaw) * PAN_SENSITIVITY;
  camera.target_y -= (-dx * sin_yaw - dy * cos_yaw) * PAN_SENSITIVITY;
}

void GuiViewport::applyOrbit(float dx, float dy) {
  camera.yaw += dx * ORBIT_SENSITIVITY;
  camera.pitch += dy * ORBIT_SENSITIVITY;
  camera.pitch = std::clamp(camera.pitch, MIN_PITCH, MAX_PITCH);
}

void GuiViewport::handleMouseMove(const GuiMouseEvent& event) {
  if (dragging_) {
    float dx = event.x - drag_last_x_;
    float dy = event.y - drag_last_y_;
    if (panning_) {
      applyPan(dx, dy);
    } else {
      applyOrbit(dx, dy);
    }
    drag_last_x_ = event.x;
    drag_last_y_ = event.y;
  }
  GuiWidget::handleMouseMove(event);
}

bool GuiViewport::handleScroll(const GuiScrollEvent& event) {
  camera.distance -= event.delta_y * ZOOM_SENSITIVITY;
  camera.distance = std::clamp(camera.distance, MIN_DISTANCE, MAX_DISTANCE);
  GuiWidget::handleScroll(event);
  return true;
}

}  // namespace eng
