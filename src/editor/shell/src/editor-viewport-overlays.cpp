#include "editor-viewport-overlays.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <editor/shell/editor-viewport-widget.h>
#include <engine/gui/gui-theme-constants.h>
#include <iterator>

namespace eng::editor {

namespace {

  /// Band colours, by `EditorNavCell`.
  constexpr GuiColor NAV_COLORS[] = {
      {0, 0, 0, 0},
      {220, 60, 50, 70},
      {235, 150, 40, 55},
      {170, 80, 220, 75},
  };
  static_assert(std::size(NAV_COLORS) ==
                static_cast<size_t>(EditorNavCell::UNREACHABLE) + 1);

  /// A line to a target the actor sees, and to one it only remembers.
  constexpr GuiColor SEEN_LINE{245, 220, 80, 230};
  /// A line to where a target was last perceived, no longer seen.
  constexpr GuiColor REMEMBERED_LINE{160, 160, 160, 150};
  /// How opaque a view cone's outline is, out of 255.
  constexpr uint8_t CONE_ALPHA = 90;
  /// Segments the arc at the end of a view cone is drawn in.
  constexpr int CONE_SEGMENTS = 16;
  /// Width of a path line, in pixels.
  constexpr float PATH_WIDTH = 2.0f;
  /// How far above an actor's head its label sits, in tiles.
  constexpr float LABEL_LIFT = 0.25f;
  /// Radians in a degree. Presentation only: the editor draws with libm.
  constexpr float RADIANS_PER_DEGREE = 0.0174532925199432958f;

  /// @p color with its alpha replaced by @p alpha.
  GuiColor withAlpha(GuiColor color, uint8_t alpha) {
    color.a = alpha;
    return color;
  }

  /// An actor's colour: its faction's.
  GuiColor factionColor(const EditorActorOverlay& actor) {
    return EDITOR_FACTION_COLORS[static_cast<size_t>(actor.faction) %
                                 std::size(EDITOR_FACTION_COLORS)];
  }

  /// A one-pixel line between two world points.
  void worldLine(GuiRendererContext& renderer, const IsoView& view,
                 std::span<const WorldPoint, 2> ends, GuiColor color) {
    const IsoPoint a = worldToScreen(view, ends[0]);
    const IsoPoint b = worldToScreen(view, ends[1]);
    renderer.emitLine({a.x, a.y, b.x, b.y, color.pack(), 1.0f});
  }

  /// How thick, on screen, a band from @p a to @p b is when one cell across
  /// it runs from @p a to @p across: the cell's extent perpendicular to the
  /// band.
  float bandThickness(IsoPoint a, IsoPoint b, IsoPoint across) {
    const float length = std::hypot(b.x - a.x, b.y - a.y);
    if (length <= 0.0f) {
      return 0.0f;
    }
    return std::fabs((b.x - a.x) * (across.y - a.y) -
                     (b.y - a.y) * (across.x - a.x)) /
           length;
  }

  /// A band along @p run: a line through the middle of its row, as thick
  /// as a cell is across the row on screen.
  void renderRun(GuiRendererContext& renderer, const IsoView& view,
                 const EditorNavOverlay& overlay, const EditorNavRun& run) {
    const float size = overlay.cell_size;
    const float z = overlay.floor_z;
    const float y =
        overlay.origin.y + (static_cast<float>(run.row) + 0.5f) * size;
    const float x0 = overlay.origin.x + static_cast<float>(run.first) * size;
    const float x1 = overlay.origin.x + static_cast<float>(run.end) * size;
    const IsoPoint a = worldToScreen(view, {x0, y, z});
    const IsoPoint b = worldToScreen(view, {x1, y, z});
    const float thick =
        bandThickness(a, b, worldToScreen(view, {x0, y + size, z}));
    renderer.emitLine({a.x, a.y, b.x, b.y,
                       NAV_COLORS[static_cast<size_t>(run.kind)].pack(),
                       thick});
  }

  /// The point @p reach tiles from @p actor at @p degrees from its facing.
  WorldPoint conePoint(const EditorActorOverlay& actor, float degrees,
                       float reach) {
    const float turn = std::atan2(actor.facing.y, actor.facing.x) +
                       degrees * RADIANS_PER_DEGREE;
    return {actor.at.x + std::cos(turn) * reach,
            actor.at.y + std::sin(turn) * reach, actor.at.z};
  }

  /// Draw @p actor's view cone: its two edges and the arc at its reach, or
  /// a circle for a view all round.
  void renderCone(GuiRendererContext& renderer, const IsoView& view,
                  const EditorActorOverlay& actor) {
    const GuiColor color = withAlpha(factionColor(actor), CONE_ALPHA);
    const float half = std::fmin(actor.view_degrees, 360.0f) * 0.5f;
    WorldPoint last = conePoint(actor, -half, actor.sight_range);
    if (half < 180.0f) {
      worldLine(renderer, view, std::array{actor.at, last}, color);
      worldLine(renderer, view,
                std::array{actor.at, conePoint(actor, half, actor.sight_range)},
                color);
    }
    for (int i = 1; i <= CONE_SEGMENTS; ++i) {
      const float along = -half + 2.0f * half * static_cast<float>(i) /
                                      static_cast<float>(CONE_SEGMENTS);
      const WorldPoint next = conePoint(actor, along, actor.sight_range);
      worldLine(renderer, view, std::array{last, next}, color);
      last = next;
    }
  }

  /// Draw @p actor's path: from its feet through every waypoint left.
  void renderPath(GuiRendererContext& renderer, const IsoView& view,
                  const EditorActorOverlay& actor) {
    const uint32_t color = factionColor(actor).pack();
    IsoPoint from = worldToScreen(view, actor.at);
    for (const WorldPoint waypoint : actor.path) {
      const IsoPoint to = worldToScreen(view, waypoint);
      renderer.emitLine({from.x, from.y, to.x, to.y, color, PATH_WIDTH});
      from = to;
    }
  }

}  // namespace

void renderNavOverlay(GuiRendererContext& renderer, const IsoView& view,
                      const EditorNavOverlay& overlay) {
  for (const EditorNavRun& run : overlay.runs) {
    renderRun(renderer, view, overlay, run);
  }
}

void renderActorOverlays(GuiRendererContext& renderer, const IsoView& view,
                         std::span<const EditorActorOverlay> actors) {
  for (const EditorActorOverlay& actor : actors) {
    renderCone(renderer, view, actor);
    renderPath(renderer, view, actor);
    if (actor.has_target) {
      worldLine(renderer, view, std::array{actor.at, actor.target},
                actor.sees_target ? SEEN_LINE : REMEMBERED_LINE);
    }
  }
}

void renderActorLabels(const GuiDrawContext& ctx, const IsoView& view,
                       std::span<const EditorActorOverlay> actors) {
  for (const EditorActorOverlay& actor : actors) {
    const IsoPoint head = worldToScreen(
        view, {actor.at.x, actor.at.y, actor.at.z + actor.height + LABEL_LIFT});
    ctx.drawText(THEME_TEXT, {head.x, head.y}, actor.label);
  }
}

}  // namespace eng::editor
